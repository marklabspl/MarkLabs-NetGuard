# Flashowanie SONOFF 4CHR3

> **SONOFF MUSI BYĆ CAŁKOWICIE ODŁĄCZONY OD 230 V PODCZAS PROGRAMOWANIA.**

Nie podłączaj równocześnie sieci energetycznej i programatora. Odsłonięta płytka może
zawierać napięcie śmiertelne. Używaj wyłącznie adaptera USB–UART z logiką i zasilaniem
**3,3 V — nigdy 5 V**.

## Połączenie

Przy całkowicie odłączonym 230 V:

```text
TX adaptera  → RX Sonoffa
RX adaptera  → TX Sonoffa
GND          → GND
3,3 V        → 3,3 V
```

Na płytach 4CH R2/R3 opisy RX/TX są według dokumentacji poprawne. Adapter musi mieć
wydajne i stabilne zasilanie 3,3 V. Jeśli zasilanie adaptera jest niewystarczające,
zastosuj osobny stabilizowany zasilacz 3,3 V ze wspólną masą — nadal bez 230 V.

## Wejście w tryb flash

1. Odłącz zasilanie 3,3 V.
2. Przytrzymaj przycisk `IO0` (wymusza GPIO0 LOW).
3. Podłącz 3,3 V / zresetuj układ, nadal trzymając przycisk.
4. Zwolnij przycisk po uruchomieniu. ESP8285 jest w bootloaderze UART.

## ESPHome Web

1. Otwórz w Chrome lub Edge: <https://web.esphome.io/>.
2. Wybierz **Connect** i port adaptera USB–UART.
3. Wybierz **Install**, następnie instalację lokalnego firmware.
4. Wskaż `netguard-sonoff-4chr3-v0.3.1.bin`.
5. Poczekaj na zakończenie zapisu i weryfikacji.

Dla ESP8266/ESP8285 wygenerowany pojedynczy obraz `.bin` jest zapisywany od adresu
`0x00000`; nie jest potrzebne scalanie kilku plików jak w nowszych ESP32.

Po flashowaniu odłącz 3,3 V, odłącz programator, zwolnij IO0 i uruchom urządzenie
normalnie. Do testu UART ponownie zachowaj pełną separację od 230 V. Terminal ustaw
na 9600 baud, 8N1 i wyślij `PING` zakończone CR lub LF; odpowiedzią ma być `PONG`.
