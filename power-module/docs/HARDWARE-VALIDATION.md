# Walidacja na sprzęcie — Power Module v0.3.1

Data: 2026-09-03

Potwierdzone na fizycznym SONOFF 4CHR3 przez użytkownika:

- firmware uruchamia komunikację UART 9600 baud, 8N1;
- komenda `PING` zwraca `PONG`;
- parser przyjmuje poprawne polecenia po zakończeniu linii;
- komenda czasowa działa i generuje oczekiwane zakończenie operacji.

Powyższe wyniki pochodzą z wersji 0.1.0. Po wgraniu 0.3.1 należy ponownie potwierdzić
`RESET <SESSION> <TX> <CH> <s>`, heartbeat, zdarzenie `CHANNEL_ON` i zmianę identyfikatora sesji.

Nadal do osobnego potwierdzenia i zapisania wyniku:

- numer/revizja nadruku PCB;
- słyszalne lub zmierzone przełączenie każdego z CH1–CH4;
- zgodność kolejności GPIO z fizycznymi zaciskami CH1–CH4;
- stan wszystkich wyjść po power-on, software reset i watchdog reset;
- zachowanie przy zaniku/brązowym spadku zasilania;
- długotrwała stabilność UART i timerów;
- testy wykonywane wyłącznie na bezpiecznym obciążeniu, przed etapem 230 V.

Nie zapisano żadnych nieprzeprowadzonych pomiarów jako wyników rzeczywistych.
