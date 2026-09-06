# Flashing SONOFF 4CHR3

The unit must be completely disconnected from mains power while programming. Never
connect mains and a programmer at the same time. Use a **3.3 V** USB–UART adapter.

With mains disconnected, connect adapter TX to Sonoff RX, adapter RX to Sonoff TX,
GND to GND and stable 3.3 V to 3.3 V. Hold IO0 low while applying 3.3 V to enter the
bootloader. Flash `netguard-sonoff-4chr3-v0.3.1.bin` at address `0x00000` using
ESPHome Web or esptool.

After flashing, disconnect power and the programmer, release IO0 and boot normally.
For a UART test use 9600 baud, 8N1 and send `PING` terminated by CR or LF. The expected
response is `PONG`.
