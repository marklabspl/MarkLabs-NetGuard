# NETGUARD-POWER 0.3.1 UART protocol

Settings: **9600 baud, 8N1, ASCII**, terminated by CR, LF or CRLF. Commands are
case-insensitive. Lines longer than 94 characters are rejected in full.

| Command | Response |
|---|---|
| `PING` | `PONG` |
| `VERSION` | `VERSION NETGUARD-POWER 0.3.1` |
| `STATUS` | session, uptime, reset reason, CH1–CH4 states and timers |
| `RESET session tx ch s` | `OK RESET session tx ch s` |
| `ALLON` | `OK ALLON` |

`tx` is a non-zero 32-bit transaction identifier, `ch` is 1–4 and `s` is 1–300.
Repeating the latest identical transaction acknowledges it without switching again.
Reusing its ID with different parameters returns `ERR TRANSACTION`. Completion is
reported as `EVENT CHANNEL_ON session tx ch`.

The module emits `HEARTBEAT session uptime states` every five seconds. `states` holds
four 0/1 digits for CH1–CH4. These are commanded logic states, not measured voltage.

`ON`, `OFF` and `PULSE` remain for diagnostics and legacy Master compatibility.
Errors: `ERR COMMAND`, `ERR SYNTAX`, `ERR CHANNEL`, `ERR TIME`, `ERR TRANSACTION`,
`ERR BUSY` and `ERR LINE TOO LONG`.
