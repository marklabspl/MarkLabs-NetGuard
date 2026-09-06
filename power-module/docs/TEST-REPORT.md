# Raport weryfikacji v0.3.1

- Kompilacja PlatformIO `sonoff_4chr3`: **PASS**.
- Arduino ESP8266 Core: 3.1.2 (PlatformIO platform espressif8266 4.2.1).
- Target: ESP8266/ESP8285, 80 MHz, flash 1 MB / 40 MHz / DOUT.
- RAM: 28 732 / 81 920 B (35,1%).
- Program: 271 159 / 958 448 B (28,3%).
- Plik BIN: 275 312 B.
- `esptool 5.4.0 image-info`: ESP8266, image v1, 1 MB, 40 MHz, DOUT,
  checksum obrazu poprawny.
- Hostowy zestaw testów behawioralnych protokołu v3: **PASS**.

Testy obejmują PING, VERSION, STATUS, transakcyjny RESET, ON 1, ON 4, OFF 1 5, OFF 4,
PULSE 1 3, PULSE 2 5, dwa niezależne PULSE, ALLON, błędną transakcję i kanał,
czas zero, nieznaną i niepełną komendę, zbyt długą linię oraz rollover millis().

PlatformIO test natywny zawierający asercje bezpośrednio wobec `protocol.cpp` jest
dołączony w `test/test_logic/test_main.cpp`. Na maszynie budującej nie było systemowego
GCC/G++, dlatego runner `native` nie mógł go uruchomić; ten sam kod `protocol.cpp`
został jednak skompilowany przez docelowy toolchain Xtensa w udanym buildzie firmware.

Bez fizycznego SONOFF 4CHR3 nie zweryfikowano elektrycznie: kolejności kanałów na
konkretnym egzemplarzu/revizji PCB, polaryzacji stopni przekaźnikowych, jakości
zasilania z danego adaptera USB–UART, rzeczywistego przełączania styków, zachowania
pod brownout/watchdog oraz transmisji na padach RX/TX. GPIO15 jest pinem strapującym
i podczas resetu musi pozostać LOW; dlatego CH4 może zostać ustawiony ON dopiero po
przekazaniu sterowania do aplikacji, a nie w fazie ROM bootloadera.
