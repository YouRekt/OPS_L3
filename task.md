# Program do całkowania numerycznego metodą Monte Carlo

W ramach laboratorium należy napisać program do całkowania numerycznego metodą Monte Carlo funkcji dx.

Program powinien wykorzystać pamięć dzieloną do współpracy z większą ilością procesów. Każdy kolejny uruchomiony program powinien przyłączyć się do obliczeń i je przyśpieszyć. Działający proces trzyma wyniki obliczeń w prywatnej pamięci co jakiś czas synchronizując się ze współdzieloną pamięcią. Po sprawdzeniu N wylosowanych próbek program powinien zaktualizować liczniki wylosowanych oraz trafionych próbek (batch processing).

W celu koordynacji obliczeń w pamięci dzielonej poza informacją o wyniku obliczeń należy przetrzymywać licznik współpracujących procesów. Przy rozpoczęciu pracy każdy proces powinien inkrementować licznik oraz dekrementować go po prawidłowym zakończeniu pracy. Kiedy proces dekrementuje go do zera, należy podsumować wyniki obliczeń i wypisać na ekran wynik aproksymacji.

## Szczegóły implementacyjne:

### Etapy:

> **[7p.]**&nbsp;&nbsp;&nbsp;&nbsp; 1. **Do współpracy między procesami wykorzystaj obiekt nazwanej pamięci dzielonej o nazwie zdefiniowanej stałą `SHMEM_NAME`.**

- Przygotuj strukturę pamięci dzielonej zawierającą licznik procesów chronionym współdzielonym mutexem.
- Napisz procedurę inicjalizacji pamięci dzielonej z prawidłową inkrementacją liczników przy uruchomieniu kolejnych procesów.
- Do wyeliminowania wyścigu pomiędzy stworzeniem pamięci dzielonej a jej inicjalizacją, użyj semafora nazwanego o nazwie zdefiniowanej stałą `SHMEM_SEMAPHORE_NAME`.
- Po prawidłowej inicjalizacji pamięci dzielonej proces wypisuje ilość współpracujących procesów, śpi 2 sekundy, a następnie kończy się. Przed zakończeniem procesu należy zniszczyć obiekt pamięci dzielonej w przypadku odłączania się ostatniego procesu.

> **[5p.]**&nbsp;&nbsp;&nbsp;&nbsp; 2. **Zaimplementuj obliczenia trzech paczek (batches) obliczeń N punktów metodą Monte Carlo.**

- Pobierz parametry do programu w taki sposób jak opisuje funkcja usage.
- Wykorzystaj dostarczoną funkcję `randomize_points` do obliczenia jednej paczki próbek.
- Dodaj do struktury pamięci dzielonej dwa liczniki opisujące ilość próbek wylosowanych oraz trafionych.
- Po obliczeniu każdej paczki zaktualizuj liczniki oraz wyświetl ich stan na standardowe wyjście.
- Po wykonaniu trzech iteracji obliczeń program powinien się skończyć z logiką niszczenia pamięci dzielonej jak w etapie pierwszym.

Uwaga: W pamięci dzielonej warto przechować zakresy całkowania, aby uniknąć przyłączenia się procesu z innymi granicami całkowania. Taki scenariusz spowoduje, że wyniki aproksymacji nie będą miały sensu.

> **[5p.]**&nbsp;&nbsp;&nbsp;&nbsp; 3. **Dodaj obsługę sygnału SIGINT, który przerwie obliczanie kolejnych paczek punktów.**

- Po otrzymaniu sygnału proces kończy obliczać aktualną paczkę, po czym przechodzi do sprawdzenia licznika pracujących procesów.
- Jeżeli proces odłącza się ostatni z pamięci dzielonej, użyj funkcji `summarize_calculations` w celu obliczenia wyniku aproksymacji, aby wypisać wyniki na standardowe wyjście.

Uwaga: W dostarczonym Makefile dodany jest nowy target clean-resources. Można go użyć do zniszczenia pamięci dzielonej oraz semafora.
