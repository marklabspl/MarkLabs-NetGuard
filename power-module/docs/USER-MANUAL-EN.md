# NETGUARD-POWER 0.3.1 user manual

## 1. Purpose

NETGUARD-POWER supplies up to four devices controlled by NETGUARD-MASTER. When the
Master requests a RESET, the Power Module switches the corresponding channel off for
the configured duration and then restores it automatically.

The Power Module has no web panel, Wi-Fi or local automation rules. Devices, timing
and rules are configured in the NETGUARD-MASTER web panel.

## 2. Required versions

- NETGUARD-POWER `0.3.1`,
- NETGUARD-MASTER `0.9.0-rc.6` or newer,
- isolated 9600-baud 8N1 UART between modules.

Update the Power Module first and immediately update the Master. Transactional RESET 0.3.1 requires Master 0.9.0-rc.6; keep automation disabled throughout the upgrade.

## 3. Commissioning

1. Flash `netguard-sonoff-4chr3-v0.3.1.bin` at address `0x00000`.
2. Cross-connect UART: Master TX to Power Module RX and Master RX to Power Module TX.
   Use the isolator specified by the project.
3. Start the Power Module and then the Master.
4. Check the **Power Module** card on the Master dashboard.
5. Wait for at least three consecutive valid heartbeats before testing RESET.

The link is ready when the dashboard shows Power Module ONLINE, an increasing
heartbeat count, a consecutive count of at least 3 and `CH1:ON` through `CH4:ON`.

## 4. Normal operation

Every channel is commanded ON after boot. The Master requests one RESET at a time.
Example for CH2 and ten seconds:

```text
RESET 99 17 2 10
OK RESET 99 17 2 10
```

CH2 becomes OFF. After ten seconds, the module restores ON and reports:

```text
EVENT CHANNEL_ON 99 17 2
```

Disconnecting UART during RESET does not stop the local timer; the channel is still
restored when the configured duration expires.

## 5. Master dashboard data

- Power Module ONLINE/OFFLINE state,
- age of the latest response,
- total and consecutive valid heartbeats,
- timeouts and invalid UART lines,
- command-in-progress state,
- CH1–CH4 states,
- detected Power Module restart count,
- latest STATUS line.

`ON` and `OFF` describe the state commanded by ESP8285. They are not measurements of
voltage at the terminals.

## 6. Post-update test

1. Open a Power Module terminal at 9600 baud, 8N1.
2. Send `VERSION`; expect `VERSION NETGUARD-POWER 0.3.1`.
3. Send `PING`; expect `PONG`.
4. Send `STATUS` and verify `R1=1` through `R4=1`.
5. Request a manual RESET of each configured channel from the Master.
6. Verify that the corresponding state changes to OFF and returns to ON.
7. Restart the Power Module and verify that the Master restart counter increases.

## 7. Troubleshooting

| Symptom | Check |
|---|---|
| Power Module OFFLINE | module supply, isolator, crossed RX/TX and 9600 8N1 |
| No `PONG` | CR/LF line termination and correct UART port |
| `ERR CHANNEL` | channel must be 1–4 |
| `ERR TIME` | duration must be 1–300 seconds |
| `ERR TRANSACTION` | ID must be non-zero and keep identical parameters |
| `ERR BUSY` | wait at least one second after the previous switch |
| Increasing `invalid_lines` | isolator grounds/sides, directions and interference |
| Logic state ON but device is off | inspect the power path; firmware cannot sense contacts |

## 8. Emergency restore

`ALLON` immediately commands all channels ON and cancels active timers. It is intended
for controlled diagnostics. Local RESET timers handle restoration during normal use.

## 9. Limitations

- no electrical relay-contact feedback,
- no voltage or current measurement,
- no persistent operation history across power loss,
- no OTA update; firmware is flashed over UART,
- at boot, ON can only be asserted after the application takes control of GPIO.

See `FLASHING-EN.md`, `DOCUMENTATION-EN.md` and `UART-PROTOCOL-EN.md` for details.
