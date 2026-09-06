# Flashing SONOFF 4CHR3

The unit must be completely disconnected from mains power while programming. Never
connect mains and a programmer at the same time. Use a **3.3 V** USB–UART adapter.
<img width="2048" height="1536" alt="Sonoff-4CH-Pro-na-ESPHome-—-pelny-przewodnik-po-flashowaniu-R2-i-R-zwieranie-pinu-do-masy-wersja-2048x1536" src="https://github.com/user-attachments/assets/e0a1bab9-a4e1-4371-9e51-9d0301f6c900" />

<img width="1024" height="768" alt="Sonoff-4CH-Pro-on-ESPHome-—-Complete-Flashing-Guide-R2-R3-1024x768" src="https://github.com/user-attachments/assets/ee6a5cd0-5cd2-470d-b192-22b6dd477b54" />


With mains disconnected, connect adapter TX to Sonoff RX, adapter RX to Sonoff TX,
GND to GND and stable 3.3 V to 3.3 V. Hold IO0 to GND to enter the
bootloader. Flash `netguard-sonoff-4chr3-v0.3.1.bin` at address `0x00000` using
ESPHome Web or esptool.

After flashing, disconnect power and the programmer, release IO0 and boot normally.
For a UART test use 9600 baud, 8N1 and send `PING` terminated by CR or LF. The expected
response is `PONG`.

