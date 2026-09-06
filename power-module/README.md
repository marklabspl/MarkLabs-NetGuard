# MarkLabs NetGuard — NETGUARD-POWER for SONOFF 4CHR3

Autonomiczny, czterokanałowy moduł wykonawczy UART dla SONOFF 4CHR3 (ESP8285).

Wersja 0.3.1 nie używa Wi-Fi, MQTT, eWeLink ani systemu automatyki domowej. Po każdym
starcie wszystkie przekaźniki są ustawiane na ON. Każde wyłączenie jest ograniczone
lokalnym timerem do 1–300 sekund. Protokół v2 używa numerowanych transakcji `RESET`,
raportuje sesję uruchomieniową, przyczynę restartu i logiczne stany kanałów.

English: autonomous four-channel UART actuator for SONOFF 4CHR3 (ESP8285). Version
0.3.1 keeps Wi-Fi and cloud services disabled, starts every channel in ON state and
uses bounded 1–300 second RESET operations with transaction identifiers.

Dokumentacja / documentation:

- `docs/UART-PROTOCOL-PL.md` / `docs/UART-PROTOCOL-EN.md`,
- `docs/USER-MANUAL-PL.md` / `docs/USER-MANUAL-EN.md`,
- `docs/DOCUMENTATION-PL.md` / `docs/DOCUMENTATION-EN.md`,
- `docs/FLASHING.md` / `docs/FLASHING-EN.md`,
- `docs/HARDWARE-VALIDATION.md` / `docs/HARDWARE-VALIDATION-EN.md`,
- `docs/TEST-REPORT.md` / `docs/TEST-REPORT-EN.md`.

## Budowanie

Wymagany PlatformIO Core:

```sh
pio run
pio test -e native
```

Artefakt PlatformIO znajduje się w `.pio/build/sonoff_4chr3/firmware.bin`.
Konfiguracja używa ESP8266 Arduino Core, ESP8285/1 MB, CPU 80 MHz, flash 40 MHz,
trybu DOUT i układu pamięci 1 MB z 64 KB dla systemu plików (nieużywanego przez firmware).

## Bezpieczeństwo

Urządzenie pracuje z napięciem sieciowym. Programowanie jest dozwolone wyłącznie po
całkowitym odłączeniu SONOFF-a od 230 V. Szczegóły: `docs/FLASHING.md`.
