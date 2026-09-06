# Hardware validation — Power Module 0.3.1

UART 9600 8N1, `PING`/`PONG`, line parsing and timed channel operation were confirmed
on physical SONOFF 4CHR3 hardware with version 0.1.0. After installing 0.3.1, verify
`RESET <SESSION> <TX> <CH> <s>`, heartbeat, `CHANNEL_ON` and session-ID changes again.

Still requiring physical confirmation: PCB revision, CH1–CH4 GPIO order, every relay,
power-on/software/watchdog reset states, brownout behavior and long-duration UART and
timer stability. Commanded GPIO state is not electrical relay-contact feedback.
