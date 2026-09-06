# Protokół UART NETGUARD-POWER 0.3.1

Parametry: **9600 baud, 8N1, ASCII**, zakończenie CR, LF albo CRLF. Polecenia nie
rozróżniają wielkości liter. Linie dłuższe niż 94 znaki są odrzucane w całości.

| Polecenie | Odpowiedź |
|---|---|
| `PING` | `PONG` |
| `VERSION` | `VERSION NETGUARD-POWER 0.3.1` |
| `STATUS` | sesja, uptime, przyczyna resetu, stany i timery CH1–CH4 |
| `RESET session tx ch s` | `OK RESET session tx ch s` |
| `ALLON` | `OK ALLON` |

`tx` jest niezerowym 32-bitowym numerem transakcji, `ch` ma zakres 1–4, a `s` 1–300.
Ponowienie ostatniej identycznej transakcji zwraca potwierdzenie bez ponownego
przełączenia. Użycie tego samego `tx` z innymi parametrami zwraca `ERR TRANSACTION`.
Po zakończeniu RESET moduł wysyła `EVENT CHANNEL_ON session tx ch`.

Co pięć sekund wysyłany jest `HEARTBEAT session uptime states`; `states` to cztery
cyfry 0/1 dla CH1–CH4. Są to logiczne stany sterowania, nie pomiar napięcia.

`ON`, `OFF` i `PULSE` pozostają dla diagnostyki i zgodności ze starszym Masterem.
Błędy: `ERR COMMAND`, `ERR SYNTAX`, `ERR CHANNEL`, `ERR TIME`, `ERR TRANSACTION`,
`ERR BUSY` i `ERR LINE TOO LONG`.
