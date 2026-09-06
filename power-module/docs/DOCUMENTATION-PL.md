# Dokumentacja techniczna NETGUARD-POWER 0.3.1

## 1. Przeznaczenie

NETGUARD-POWER jest czterokanałowym modułem wykonawczym projektu MarkLabs NetGuard.
Firmware jest przeznaczony dla SONOFF 4CHR3 z ESP8285 i współpracuje z
NETGUARD-MASTER 0.9.0-rc.6 lub nowszym przez izolowany UART.

Moduł nie podejmuje decyzji diagnostycznych. Master wybiera urządzenie i czas RESET,
a Power Module bezpiecznie wykonuje czasowe wyłączenie właściwego kanału.

## 2. Najważniejsze parametry

| Parametr | Wartość |
|---|---|
| Mikrokontroler | ESP8285, 80 MHz |
| Pamięć flash | 1 MB, DOUT, 40 MHz |
| Kanały | CH1–CH4 |
| GPIO przekaźników | CH1=GPIO12, CH2=GPIO5, CH3=GPIO4, CH4=GPIO15 |
| UART aplikacji | 9600 baud, 8N1, ASCII |
| Czas RESET | 1–300 s |
| Heartbeat | co 5 s |
| Watchdog programowy | 8 s |
| Wi-Fi i usługi chmurowe | wyłączone |

## 3. Model działania

Po uruchomieniu firmware ustawia wszystkie cztery kanały w logiczny stan ON. RESET
ustawia wybrany kanał OFF i uruchamia niezależny, nieblokujący timer. Po upływie czasu
moduł sam przywraca ON, nawet jeśli Master przestał odpowiadać.

Każdy kanał ma osobny timer. Parser odrzuca całą nieprawidłową linię i nigdy nie
wykonuje jej części. Operacje RESET są ograniczone do 300 sekund, a kolejne
przełączenie tego samego kanału nie może rozpocząć się wcześniej niż po jednej
sekundzie od poprzedniego przełączenia.

Stany raportowane przez UART są stanami zadanymi GPIO. Sprzęt nie dostarcza firmware
elektrycznego potwierdzenia położenia styku ani napięcia na wyjściu.

## 4. Protokół transakcyjny

Produkcja używa polecenia:

```text
RESET <SESJA> <ID> <CH> <SEKUNDY>
```

Przykład:

```text
RESET 99 17 2 10
OK RESET 99 17 2 10
EVENT CHANNEL_ON 99 17 2
```

ID jest niezerową liczbą 32-bitową nadawaną przez Master. Odpowiedź zostaje uznana
tylko wtedy, gdy ID, kanał i czas są identyczne. Powtórzenie ostatniej identycznej
transakcji jest idempotentne: moduł powtarza potwierdzenie, lecz nie przełącza kanału
ponownie. Pełny opis znajduje się w `UART-PROTOCOL-PL.md`.

## 5. Telemetria

Po starcie moduł wysyła:

```text
BOOT NETGUARD-POWER 0.3.1 SESSION=12AB34CD RESET=6
STATUS SESSION=12AB34CD UPTIME=0 RESET=6 R1=1 T1=0 R2=1 T2=0 R3=1 T3=0 R4=1 T4=0
```

Co pięć sekund pojawia się:

```text
HEARTBEAT 12AB34CD 25 1111
```

Pola oznaczają kolejno sesję, uptime w sekundach i stany CH1–CH4. Zmiana SESSION
pozwala Masterowi wykryć restart Power Module.

## 6. Zgodność

Power Module 0.3.1 nadal rozumie `PULSE`, `ON`, `OFF`, `ALLON`, `PING`, `VERSION`,
`STATUS` i `HELP`. Dzięki temu można najpierw zaktualizować Power Module, gdy stary
Master nadal pracuje. Master 0.9.0-rc.6 używa już wyłącznie transakcyjnego RESET.

## 7. Budowanie i testy

```text
platformio run
python tests/run_logic_tests.py
```

Ostatni zweryfikowany build wykorzystuje 28 732 B RAM i 271 159 B programu. Parser,
obsługa zakresów i rollover `millis()` przechodzą test hostowy, a kod produkcyjny
kompiluje się docelowym toolchainem ESP8266. Testy przekaźników i zakłóceń UART muszą
zostać wykonane na fizycznym module.

## 8. Aktualizacja

Plik `netguard-sonoff-4chr3-v0.3.1.bin` zapisuje się od adresu `0x00000`. Aktualizuj
najpierw NETGUARD-POWER, potem NETGUARD-MASTER. Szczegóły znajdują się w
`FLASHING.md`.
