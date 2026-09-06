# Instrukcja obsługi NETGUARD-POWER 0.3.1

## 1. Do czego służy moduł

NETGUARD-POWER zasila maksymalnie cztery urządzenia sterowane przez NETGUARD-MASTER.
Gdy Master zdecyduje o wykonaniu RESET, Power Module wyłącza odpowiedni kanał na
ustawiony czas, a następnie sam go włącza.

Power Module nie ma panelu WWW, Wi-Fi ani własnych reguł. Konfigurację urządzeń,
czasów i automatyzacji wykonuje się w panelu NETGUARD-MASTER.

## 2. Wymagane wersje

- NETGUARD-POWER: `0.3.1`,
- NETGUARD-MASTER: `0.9.0-rc.6` lub nowszy,
- komunikacja między modułami: izolowany UART 9600 baud, 8N1.

Aktualizuj najpierw Power Module, a następnie niezwłocznie Master. Transakcyjny RESET 0.3.1 wymaga Mastera 0.9.0-rc.6; w trakcie aktualizacji automatyka musi pozostać wyłączona.

## 3. Uruchomienie

1. Wgraj `netguard-sonoff-4chr3-v0.3.1.bin` od adresu `0x00000`.
2. Połącz krzyżowo UART: TX Mastera do RX Power Module oraz RX Mastera do TX Power
   Module. Zastosuj izolator zgodnie z projektem.
3. Uruchom najpierw Power Module, a następnie Master.
4. Na panelu Mastera sprawdź kartę **Power Module**.
5. Poczekaj na co najmniej trzy poprawne heartbeat'y przed wykonaniem RESET.

Połączenie jest gotowe, gdy panel pokazuje Power Module jako ONLINE, rosnący licznik
heartbeatów, serię co najmniej 3 poprawnych komunikatów i stany `CH1:ON`–`CH4:ON`.

## 4. Normalna praca

Po każdym starcie wszystkie kanały przechodzą do ON. Master może zlecić tylko jedną
operację RESET naraz. Przykład dla CH2 i czasu 10 sekund:

```text
RESET 99 17 2 10
OK RESET 99 17 2 10
```

CH2 przechodzi do OFF. Po 10 sekundach moduł sam przywraca ON i raportuje:

```text
EVENT CHANNEL_ON 99 17 2
```

Odłączenie UART podczas RESET nie zatrzymuje lokalnego timera i kanał nadal zostanie
włączony po ustawionym czasie.

## 5. Informacje widoczne w Masterze

- ONLINE/OFFLINE Power Module,
- wiek ostatniej odpowiedzi,
- liczba oraz seria poprawnych heartbeatów,
- timeouty i błędne linie UART,
- komenda w toku,
- stany CH1–CH4,
- liczba wykrytych restartów Power Module,
- ostatnia linia STATUS.

`ON` i `OFF` opisują stan zadany przez ESP8285. Nie są pomiarem napięcia na zaciskach.

## 6. Test po aktualizacji

1. Otwórz terminal Power Module: 9600 baud, 8N1.
2. Wyślij `VERSION`; oczekuj `VERSION NETGUARD-POWER 0.3.1`.
3. Wyślij `PING`; oczekuj `PONG`.
4. Wyślij `STATUS` i sprawdź `R1=1` do `R4=1`.
5. Z Mastera wykonaj ręczny RESET każdego skonfigurowanego kanału.
6. Sprawdź zmianę odpowiedniego stanu na OFF i późniejszy powrót do ON.
7. Zrestartuj Power Module i sprawdź wzrost licznika restartów w Masterze.

## 7. Diagnostyka

| Objaw | Sprawdzenie |
|---|---|
| Power Module OFFLINE | zasilanie modułu, izolator, skrzyżowanie RX/TX, 9600 8N1 |
| Brak `PONG` | zakończenie komendy CR/LF i właściwy port UART |
| `ERR CHANNEL` | kanał musi mieć numer 1–4 |
| `ERR TIME` | czas musi wynosić 1–300 sekund |
| `ERR TRANSACTION` | ID musi być niezerowe i nie może zmieniać parametrów |
| `ERR BUSY` | odczekaj co najmniej sekundę od poprzedniego przełączenia |
| Rosną `invalid_lines` | sprawdź masę po obu stronach izolatora, kierunki oraz zakłócenia |
| Kanał logicznie ON, ale urządzenie nie działa | sprawdź tor zasilania; firmware nie mierzy styków |

## 8. Powrót awaryjny

Komenda `ALLON` natychmiast ustawia wszystkie kanały ON i anuluje aktywne timery.
Jest przeznaczona do kontrolowanej diagnostyki. W normalnej pracy przywracaniem
kanałów zarządzają lokalne timery RESET.

## 9. Ograniczenia

- brak elektrycznego pomiaru stanu styków,
- brak pomiaru napięcia i prądu,
- brak pamięci historii po zaniku zasilania,
- brak aktualizacji OTA — firmware wgrywa się przez UART,
- przy starcie stan ON może zostać ustawiony dopiero po przejęciu GPIO przez aplikację.

Procedura programowania znajduje się w `FLASHING.md`, a szczegóły techniczne w
`DOCUMENTATION-PL.md` i `UART-PROTOCOL-PL.md`.
