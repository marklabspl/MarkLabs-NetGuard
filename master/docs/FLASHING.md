# Flashowanie WT32-ETH01 — build produkcyjny

Firmware jest przeznaczony wyłącznie dla klasycznego WT32-ETH01 z ESP32 i LAN8720.
Nie używać go z WT32-ETH01-C3 ani inną płytką o podobnej nazwie.

## Połączenie programatora 3,3 V

```text
USB–UART TX  → RX0 / GPIO3
USB–UART RX  → TX0 / GPIO1
USB–UART GND → GND
zasilacz 5 V → pin 5V i GND WT32
```

Logika UART musi mieć 3,3 V. Zalecane jest niezależne, stabilne zasilanie 5 V zamiast
obciążania wyjścia 3,3 V przypadkowego programatora.

Tryb programowania:

1. Połącz IO0 z GND.
2. Zresetuj WT32 przez krótkie wymuszenie EN LOW albo cykl zasilania.
3. Rozłącz IO0 od GND po wejściu do bootloadera.
4. Wgraj obraz.

## Obrazy

- `netguard-master-wt32-eth01-v<version>-merged.bin` — pełny obraz do zapisania od
  `0x0000`, przeznaczony do pierwszej instalacji przez programator szeregowy.
- `netguard-master-wt32-eth01-v<version>-app.bin` — sama aplikacja pod `0x10000`,
  przeznaczona do aktualizacji przez zakładkę OTA albo dla narzędzi świadomie
  obsługujących partycje ESP32.

Po zapisie uruchom płytkę bez IO0 zwartego do masy. Konsola diagnostyczna działa z
prędkością 115200 baud. Build zgłasza `PRODUCTION`.
