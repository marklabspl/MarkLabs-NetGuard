# NETGUARD-POWER 0.3.1 technical documentation

## 1. Purpose

NETGUARD-POWER is the four-channel actuator in the MarkLabs NetGuard system. The
firmware targets SONOFF 4CHR3 with ESP8285 and communicates with NETGUARD-MASTER
0.9.0-rc.6 or newer over isolated UART.

The module makes no diagnostic decisions. The Master selects a device and RESET
duration; the Power Module performs a bounded power interruption on the corresponding
channel.

## 2. Key parameters

| Parameter | Value |
|---|---|
| MCU | ESP8285, 80 MHz |
| Flash | 1 MB, DOUT, 40 MHz |
| Channels | CH1–CH4 |
| Relay GPIO | CH1=GPIO12, CH2=GPIO5, CH3=GPIO4, CH4=GPIO15 |
| Application UART | 9600 baud, 8N1, ASCII |
| RESET duration | 1–300 s |
| Heartbeat | every 5 s |
| Software watchdog | 8 s |
| Wi-Fi and cloud services | disabled |

## 3. Operating model

At boot, firmware commands all four channels ON. RESET commands one channel OFF and
starts an independent non-blocking timer. The module restores ON when the timer
expires, even when the Master is no longer communicating.

Every channel has its own timer. The parser rejects an invalid line in full and never
executes a partial command. RESET duration is limited to 300 seconds. A new OFF
transition on the same channel cannot start less than one second after its previous
transition.

UART values represent commanded GPIO states. Hardware provides no electrical relay
contact or output-voltage feedback to firmware.

## 4. Transactional protocol

Production uses:

```text
RESET <SESSION> <ID> <CH> <SECONDS>
```

Example:

```text
RESET 99 17 2 10
OK RESET 99 17 2 10
EVENT CHANNEL_ON 99 17 2
```

ID is a non-zero 32-bit number assigned by the Master. An acknowledgement is accepted
only when ID, channel and duration match. Repeating the latest identical transaction
is idempotent: the module repeats its acknowledgement without switching again. See
`UART-PROTOCOL-EN.md` for the complete protocol.

## 5. Telemetry

Boot output:

```text
BOOT NETGUARD-POWER 0.3.1 SESSION=12AB34CD RESET=6
STATUS SESSION=12AB34CD UPTIME=0 RESET=6 R1=1 T1=0 R2=1 T2=0 R3=1 T3=0 R4=1 T4=0
```

Periodic output:

```text
HEARTBEAT 12AB34CD 25 1111
```

Fields contain the session, uptime in seconds and CH1–CH4 states. A changed SESSION
allows the Master to detect a Power Module restart.

## 6. Compatibility

Power Module 0.3.1 still accepts `PULSE`, `ON`, `OFF`, `ALLON`, `PING`, `VERSION`,
`STATUS` and `HELP`. The Power Module can therefore be updated first while an older
Master remains active. Master 0.9.0-rc.6 uses transactional RESET.

## 7. Build and verification

```text
platformio run
python tests/run_logic_tests.py
```

The verified build uses 28,708 bytes of RAM and 270,983 bytes of program storage.
Parser, range and `millis()` rollover checks pass the host suite, and production code
compiles with the target ESP8266 toolchain. Relay and disturbed-UART behavior still
require tests on physical hardware.

## 8. Updating

Flash `netguard-sonoff-4chr3-v0.3.1.bin` at `0x00000`. Update NETGUARD-POWER first and
NETGUARD-MASTER second. See `FLASHING-EN.md` for details.
