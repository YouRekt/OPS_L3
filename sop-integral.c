#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "macros.h"

// Use those names to cooperate with $make clean-resources target.
#define SHMEM_SEMAPHORE_NAME "/sop-shmem-sem"
#define SHMEM_NAME "/sop-shmem"
#define SHM_SIZE                                                               \
  1024 // sizeof(pthread_mutex_t) + sizeof(unsigned int) + sizeof(double)
// #define BATCHES 3
typedef struct timespec timespec_t;

void msleep(unsigned int milisec) {
  time_t sec = (int)(milisec / 1000);
  milisec = milisec - (sec * 1000);
  timespec_t req = {0};
  req.tv_sec = sec;
  req.tv_nsec = milisec * 1000000L;
  if (nanosleep(&req, &req))
    ERR("nanosleep");
}

sig_atomic_t process_batches = 1;

int sethandler(void (*f)(int), int sigNo) {
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = f;
  if (-1 == sigaction(sigNo, &act, NULL))
    return -1;
  return 0;
}

typedef struct {
  int process_number;
  int hit_points;
  int randomized_points;
  int a;
  int b;
} shmem_struct_t;

// Values of this function is in range (0,1]
double func(double x) {
  usleep(2000);
  return exp(-x * x);
}

/**
 * It counts hit points by Monte Carlo method.
 * Use it to process one batch of computation.
 * @param N Number of points to randomize
 * @param a Lower bound of integration
 * @param b Upper bound of integration
 * @return Number of points which was hit.
 */
int randomize_points(int N, float a, float b) {
  int i = 0, counter = 0;
  for (; i < N; ++i) {
    double rand_x = ((double)rand() / RAND_MAX) * (b - a) + a;
    double rand_y = ((double)rand() / RAND_MAX);
    double real_y = func(rand_x);

    if (rand_y <= real_y)
      counter++;
  }
  return counter;
}

/**
 * This function calculates approxiamtion of integral from counters of hit and
 * total points.
 * @param total_randomized_points Number of total randomized points.
 * @param hit_points NUmber of hit points.
 * @param a Lower bound of integration
 * @param b Upper bound of integration
 * @return The approximation of intergal
 */
double summarize_calculations(uint64_t total_randomized_points,
                              uint64_t hit_points, float a, float b) {
  return (b - a) * ((double)hit_points / (double)total_randomized_points);
}

/**
 * This function locks mutex and can sometimes die (it has 2% chance to die).
 * It cannot die if lock would return an error.
 * It doesn't handle any errors. It's users responsibility.
 * Use it only in STAGE 4.
 *
 * @param mtx Mutex to lock
 * @return Value returned from pthread_mutex_lock.
 */
int random_death_lock(pthread_mutex_t *mtx) {
  int ret = pthread_mutex_lock(mtx);
  if (ret)
    return ret;

  // 2% chance to die
  if (rand() % 50)
    abort();
  return ret;
}

void sig_handler(int sig) {
  UNUSED(sig);
  process_batches = 0;
}

void usage(char *argv[]) {
  printf("%s a b N - calculating integral with multiple processes\n", argv[0]);
  printf("a - Start of segment for integral (default: -1)\n");
  printf("b - End of segment for integral (default: 1)\n");
  printf("N - Size of batch to calculate before report to shared memory "
         "(default: 1000)\n");
}

int main(int argc, char *argv[]) {
  int a = -1, b = 1, N = 1000;
  if (argc > 4)
    usage(argv);
  if (argc > 1)
    a = atoi(argv[1]);
  if (argc > 2)
    b = atoi(argv[2]);
  if (argc > 3)
    N = atoi(argv[3]);

  // UNUSED(a);
  // UNUSED(b);
  // UNUSED(N);

  sem_t *semaphore;

  if (SEM_FAILED ==
      (semaphore = sem_open(SHMEM_SEMAPHORE_NAME, O_CREAT, 0666, 1)))
    ERR("sem_open");

  if (-1 == sem_wait(semaphore))
    ERR("sem_wait");

  int shm_fd;

  if (-1 == (shm_fd = shm_open(SHMEM_NAME, O_CREAT | O_RDWR, 0666)))
    ERR("shm_open");
  if (-1 == ftruncate(shm_fd, SHM_SIZE))
    ERR("ftruncate");

  char *shm_ptr;
  if (MAP_FAILED ==
      (shm_ptr = (char *)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
                              MAP_SHARED, shm_fd, 0)))
    ERR("mmap");

  pthread_mutex_t *mutex = (pthread_mutex_t *)shm_ptr;
  shmem_struct_t *shmem = (shmem_struct_t *)(shm_ptr + sizeof(pthread_mutex_t));

  pthread_mutexattr_t mutex_attr;
  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
  pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST);
  pthread_mutex_init(mutex, &mutex_attr);

  if (sethandler(sig_handler, SIGINT))
    ERR("Setting SIGINT handler");

  if (-1 == sem_post(semaphore))
    ERR("sem_post");

  int error;
  if (0 != (error = pthread_mutex_lock(mutex))) {
    if (EOWNERDEAD == error)
      pthread_mutex_consistent(mutex);
    else
      ERR("pthread_mutex_lock");
  }
  fprintf(stderr, "a: %d\tb: %d\n", shmem->a, shmem->b);
  fprintf(stderr, "input a: %d\tinput b: %d\n", a, b);
  shmem->process_number++;
  if (0 == shmem->a && 0 == shmem->b) {
    shmem->a = a;
    shmem->b = b;
  }
  fprintf(stderr, "Current working processes: [%d]\n", shmem->process_number);
  fprintf(stderr, "a: %d\tb: %d\n", shmem->a, shmem->b);

  pthread_mutex_unlock(mutex);

  // msleep(2000);
  // int batch_number = BATCHES;
  if (shmem->a == a && shmem->b == b) {
    while (process_batches) {
      int hit_points = randomize_points(N, a, b);
      if (0 != (error = pthread_mutex_lock(mutex))) {
        if (EOWNERDEAD == error)
          pthread_mutex_consistent(mutex);
        else
          ERR("pthread_mutex_lock");
      }
      shmem->hit_points += hit_points;
      shmem->randomized_points += N;
      fprintf(stderr, "Hit points: %d\tTotal points: %d\n", shmem->hit_points,
              shmem->randomized_points);
      pthread_mutex_unlock(mutex);
    }
  } else
    fprintf(stderr, "Wrong integration bounds!\n");

  if (0 != (error = pthread_mutex_lock(mutex))) {
    if (EOWNERDEAD == error)
      pthread_mutex_consistent(mutex);
    else
      ERR("pthread_mutex_lock");
  }

  shmem->process_number--;

  pthread_mutex_unlock(mutex);

  sem_close(semaphore);
  if (0 == shmem->process_number) {
    fprintf(stderr, "Summarized calculations: %f\n",
            summarize_calculations(shmem->randomized_points, shmem->hit_points,
                                   shmem->a, shmem->b));
    pthread_mutexattr_destroy(&mutex_attr);
    pthread_mutex_destroy(mutex);
    munmap(shm_ptr, SHM_SIZE);
    shm_unlink(SHMEM_NAME);
    sem_unlink(SHMEM_SEMAPHORE_NAME);
  }

  return EXIT_SUCCESS;
}