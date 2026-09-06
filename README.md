# MarkLabs.pl NetGuard

MarkLabs.pl NetGuard is a self-contained network monitoring and power-recovery appliance. It checks configurable network targets, evaluates dependency-aware recovery rules, and can power-cycle one of four connected devices when the configured evidence indicates a failure.

The system consists of two controllers:

- **Master** — WT32-ETH01 (ESP32 + LAN8720): diagnostics, automation, web UI, configuration, event logs, OTA, and Home Assistant MQTT integration.
- **Power Module** — SONOFF 4CHR3 (ESP8266/ESP8285): four relay channels and acknowledged RESET execution.

The controllers communicate through an isolated UART link. Network decisions remain separate from relay timing, and every recovery transaction is explicitly acknowledged.

> **Compatible release pair:** NETGUARD-MASTER `0.9.0-rc.6`, NETGUARD-POWER `0.3.1`, UART protocol v3 at 9600 baud, 8N1.

## Contents

- [Features](#features)
- [Architecture and supported hardware](#architecture-and-supported-hardware)
- [Safety and security](#safety-and-security)
- [Hardware connections](#hardware-connections)
- [Firmware installation](#firmware-installation)
- [First start](#first-start)
- [Operating principle](#operating-principle)
- [Configuration model](#configuration-model)
- [Recovery safeguards](#recovery-safeguards)
- [Web UI, backup, and OTA](#web-ui-backup-and-ota)
- [Home Assistant](#home-assistant)
- [UART protocol](#uart-protocol)
- [Building and testing](#building-and-testing)
- [Troubleshooting](#troubleshooting)
- [Documentation](#documentation)

## Features

- Four independently named and configured powered-device channels.
- Up to 12 reusable diagnostic targets, separate from powered devices.
- PING, TCP, HTTP, HTTPS, and DNS checks.
- Up to 8 rules with 1–4 conditions each.
- `ALL`/`ANY` matching, expected `UP`/`DOWN` state, and 1–10 consecutive-cycle confirmation.
- Controlled RESET transactions with power-off, stabilization, retry-delay, hourly-limit, and global-spacing safeguards.
- Responsive English/Polish web panel with live system, target, device, rule, Ethernet, UART, and recovery data.
- DHCP or static IPv4 addressing.
- Configuration backup/restore and Master web OTA.
- Home Assistant integration through MQTT Discovery.
- Redundant persistent configuration and persistent recovery counters.
- Versioned, acknowledged UART protocol and prebuilt release binaries with SHA-256 manifests.

## Architecture and supported hardware

```text
                Ethernet / management LAN
                          |
                 +--------+---------+
                 | NetGuard Master  |
                 | WT32-ETH01       |
                 | diagnostics      |
                 | rules / web/MQTT |
                 +--------+---------+
                          |
                 isolated UART v3
                   9600 baud, 8N1
                          |
                 +--------+---------+
                 | Power Module     |
                 | SONOFF 4CHR3     |
                 | CH1 CH2 CH3 CH4  |
                 +--+---+---+---+---+
                    |   |   |   |
                 powered network devices
```

The Master target is the classic **WT32-ETH01** with ESP32 and LAN8720, not the similarly named C3 board. It requires a stable 5 V supply. Initial flashing and service access require a 3.3 V logic-level USB-to-UART adapter.

The Power Module target is the **SONOFF 4CHR3** based on ESP8266/ESP8285. It must run the matching NETGUARD-POWER firmware; stock SONOFF firmware is not compatible with the NetGuard protocol.

## Safety and security

The firmware controls equipment capable of switching hazardous voltage. Firmware installation, enclosure work, wiring, isolation, grounding, protection, and production deployment must be performed by a qualified person and comply with applicable regulations.

- Fully disconnect the Power Module from mains before connecting a programmer or touching its board.
- Never power it from mains and a USB-to-UART adapter simultaneously.
- Use galvanic isolation between the Master UART and a mains-powered relay controller.
- Perform initial firmware and communication tests at safe low voltage.

NetGuard is intended for a trusted management network. The web panel uses HTTP Basic Authentication over HTTP, HTTPS diagnostics do not validate the remote certificate, MQTT TLS currently uses insecure certificate mode, and OTA images are not cryptographically signed. Do not expose the device directly to the Internet. Use a protected management VLAN and VPN for remote access. See [SECURITY.md](SECURITY.md).

## Hardware connections

### Master service UART

Use a 3.3 V logic-level adapter and power the WT32-ETH01 from a stable 5 V source:

| USB-to-UART | WT32-ETH01 |
|---|---|
| TX | RX0 / GPIO3 |
| RX | TX0 / GPIO1 |
| GND | GND |

Hold `IO0` low while resetting or powering the board to enter the ROM bootloader, then release it. The Master service console uses **115200 baud, 8N1**.

### Runtime UART

| WT32-ETH01 | Direction | Power Module |
|---|---:|---|
| GPIO33 (TX) | Master → Power | RX through isolator |
| GPIO32 (RX) | Power → Master | TX through isolator |
| reference | isolator-specific | reference |

Runtime communication uses **9600 baud, 8N1**. TX and RX must be crossed. Follow the isolator manufacturer's requirements for its supplies, direction pins, enables, and two isolated ground domains.

### Power Module programming UART

With mains fully disconnected:

| USB-to-UART | SONOFF 4CHR3 |
|---|---|
| TX | RX |
| RX | TX |
| GND | GND |
| regulated 3.3 V | 3.3 V |

Hold `IO0` low while applying 3.3 V to enter the bootloader. Never apply 5 V to the 3.3 V rail or UART pins.

## Firmware installation

### Release files

Master `0.9.0-rc.6`:

- [`netguard-master-wt32-eth01-v0.9.0-rc.6-merged.bin`](master/firmware/netguard-master-wt32-eth01-v0.9.0-rc.6-merged.bin) — full serial image for `0x0000`.
- [`netguard-master-wt32-eth01-v0.9.0-rc.6-app.bin`](master/firmware/netguard-master-wt32-eth01-v0.9.0-rc.6-app.bin) — application image for web OTA or `0x10000` with a known-compatible partition layout.
- [`netguard-master-wt32-eth01-v0.9.0-rc.6.zip`](master/firmware/netguard-master-wt32-eth01-v0.9.0-rc.6.zip) — release package.
- [`SHA256SUMS-0.9.0-rc.6.txt`](master/firmware/SHA256SUMS-0.9.0-rc.6.txt) — checksums.

Power Module `0.3.1`:

- [`netguard-sonoff-4chr3-v0.3.1.bin`](power-module/firmware/netguard-sonoff-4chr3-v0.3.1.bin) — serial image for `0x00000`.
- [`MarkLabs-NetGuard-Power-Sonoff-4CHR3-v0.3.1.zip`](power-module/firmware/MarkLabs-NetGuard-Power-Sonoff-4CHR3-v0.3.1.zip) — release package.
- [`SHA256SUMS-0.3.1.txt`](power-module/firmware/SHA256SUMS-0.3.1.txt) — checksums.

Verify every image before flashing. On PowerShell:

```powershell
Get-FileHash .\firmware-file.bin -Algorithm SHA256
```

### Required update order

1. Disable Master automation.
2. Flash/update the **Power Module first**.
3. Flash/update the **Master immediately afterward**.
4. Confirm protocol compatibility and Power Module online state.
5. Test one channel manually in a controlled environment.
6. Re-enable automation only after both versions are verified.

Never leave automation enabled while the controllers run mismatched protocol versions.

### Install the Power Module

1. Fully disconnect mains and verify the board is not energized.
2. Connect the 3.3 V programmer as documented above.
3. Hold `IO0` low and apply 3.3 V.
4. Flash `netguard-sonoff-4chr3-v0.3.1.bin` at `0x00000`.
5. Remove power, disconnect the programmer, and release `IO0`.
6. At safe low voltage, open a 9600-baud terminal and send `PING` followed by Enter. The required response is `PONG`.

See [Power Module flashing](power-module/docs/FLASHING-EN.md) for full instructions.

### Install the Master

1. Connect the service UART and a stable 5 V supply.
2. Hold `IO0` low while resetting/powering the board, then release it.
3. Flash the merged image at `0x0000`.
4. Reset and monitor the console at 115200 baud.
5. Confirm the `NETGUARD-MASTER 0.9.0-rc.6` boot banner.

Example with esptool (replace `COM5` with the actual port):

```bash
esptool.py --chip esp32 --port COM5 --baud 460800 write_flash 0x0000 master/firmware/netguard-master-wt32-eth01-v0.9.0-rc.6-merged.bin
```

Use the merged image on a blank device. The application image alone does not contain the complete bootloader and partition layout.

## First start

1. Connect the Master to Ethernet and power it on.
2. DHCP is enabled by default. Find the address in the DHCP leases or on the 115200-baud console.
3. Open `http://<device-ip>/`.
4. Sign in with username `admin` and password `admin`.
5. Immediately set a new password of at least 8 characters.
6. Keep automation disabled while defining targets, devices, and rules.
7. Confirm Ethernet, UART, and Power Module status on the dashboard.
8. Run manual diagnostics and one controlled manual RESET.
9. Enable tested rules, then enable global automation.

Static IPv4 mode requires a valid address, mask, gateway, and DNS configuration. After changing network settings, reconnect at the new address. The service UART is the primary diagnostic path if the panel becomes unreachable.

## Operating principle

NetGuard separates **what is monitored** from **what is powered**. A target is an address or service to check. A powered device is a physical relay channel. A rule connects evidence from targets to a recovery action.

This supports dependency-aware logic such as:

> If access point `192.168.1.99` is down while router `192.168.1.1` and DNS server `8.8.8.8` are up, RESET the device assigned to channel 1.

For every diagnostic cycle, the Master:

1. Runs enabled target checks and records state, latency, errors, and counters.
2. Evaluates each enabled rule against the coherent cycle.
3. Requires the configured number of consecutive matching cycles.
4. Checks device, channel, timing, limit, UART, and automation safeguards.
5. Starts a RESET with a unique boot-session and transaction ID.
6. Waits for the Power Module acknowledgment and final `CHANNEL_ON` event.
7. Records the result, persists recovery counters, and starts stabilization/retry timing.

A transmitted or acknowledged command is not yet a successful recovery. Success is recorded only after `EVENT CHANNEL_ON` confirms that power was restored.

## Configuration model

### Diagnostic targets

Up to **12** independent targets may be configured with a name, icon, address, method, timing, and method-specific parameters:

- **PING** — ICMP reachability and latency.
- **TCP** — connection to a selected TCP port.
- **HTTP** — HTTP request and response evaluation.
- **HTTPS** — HTTPS request without remote-certificate validation.
- **DNS** — lookup through a configured DNS server, so DNS availability participates in rule logic instead of being telemetry only.

Factory targets include `1.1.1.1` and `8.8.8.8` using PING; all targets are editable.

### Powered devices

Up to **four** powered devices map one-to-one to physical channels `CH1`–`CH4`. Names, icons, purpose, timing, and channel assignments are editable. One physical channel cannot be assigned to multiple devices.

### Rules

Up to **8** rules may be defined. Each contains:

- one powered device to RESET;
- 1–4 target conditions with expected `UP` or `DOWN` state;
- `ALL` mode (every condition matches) or `ANY` mode (at least one matches);
- 1–10 required consecutive matching cycles;
- an enabled/disabled state.

Example:

```text
Rule: Recover office access point
Action: RESET "Office AP" on CH1
Mode: ALL
Conditions:
  - Office AP / PING / expected DOWN
  - Main router / PING / expected UP
  - Public DNS / DNS / expected UP
Consecutive cycles: 3
```

Use positive upstream conditions to prevent unnecessary restarts when a router or the entire Internet connection is unavailable.

### Manual RESET

A manual RESET bypasses diagnostic conditions only. It still obeys transaction protection, UART availability, channel validity, timing, and recovery safeguards.

## Recovery safeguards

| Setting | Range | Purpose |
|---|---:|---|
| Power-off time | 1–300 s | How long the channel remains off. |
| Stabilization | 0–3600 s | Boot/recovery time before new decisions. |
| Retry delay | 0–86400 s | Minimum delay before another automatic attempt. |
| Hourly limit | 0–20 | Rolling-hour recovery limit; `0` disables automatic recovery for that device. |
| Global spacing | 15 s | Minimum separation between system-wide recovery operations. |

The engine also prevents duplicate channel assignments, overlapping RESETs, stale UART transactions, and actions while global automation or the selected rule/device is disabled. Recovery history and counters survive normal Master restarts.

## Web UI, backup, and OTA

The bilingual dashboard displays firmware and uptime, Ethernet addressing, Power Module heartbeat/UART health, target results and latency, powered-device state, rule conditions and block reasons, active recovery timing, counters, and recent events.

Configuration is validated before storage. The Master uses two NVS records with CRC validation to reduce the risk of accepting a partial or corrupted write.

Backups contain operational configuration but intentionally exclude panel and MQTT passwords. Restore disables automation during validation and application. Store credentials separately.

For OTA, upload the Master `*-app.bin` file. Updates are rejected during an active RESET, for an inconsistent product filename, invalid ESP32 header, or insufficient write space. Do not remove power during writing. OTA currently has no signature verification or automatic rollback; use only a checksum-verified trusted image.

## Home Assistant

The Master publishes MQTT Discovery entities for system state, targets, powered devices, rules, and supported controls. Use a dedicated MQTT user limited to the NetGuard topic namespace and keep the broker on a trusted network. Configure the broker, credentials, base topic, and optional TLS in the panel, then verify entity availability before enabling controls.

See [Home Assistant integration](master/docs/HOME-ASSISTANT-EN.md) for configuration and entity details.

## UART protocol

The UART v3 protocol is line-oriented ASCII. Commands are case-insensitive, accept CR/LF/CRLF, and reject lines longer than 94 characters.

```text
Master -> Power: PING
Power  -> Master: PONG

Master -> Power: RESET <SESSION> <TX> <CH> <SECONDS>
Power  -> Master: OK RESET <SESSION> <TX> <CH> <SECONDS>
Power  -> Master: EVENT CHANNEL_ON <SESSION> <TX> <CH>
```

`SESSION` identifies the Master boot session; `TX` identifies the transaction. Power `0.3.1` rejects stale IDs in the same session and accepts a duplicate only while that exact transaction is active. A heartbeat is emitted every 5 seconds. See the [UART specification](power-module/docs/UART-PROTOCOL-EN.md).

## Building and testing

Requirements are PlatformIO Core, Python 3, and the toolchains installed by the declared PlatformIO environments.

```bash
git clone <repository-url>
cd MarkLabs-NetGuard

platformio run -d power-module -e sonoff_4chr3
platformio run -d master -e wt32_eth01

python master/tests/run_regression_tests.py
python power-module/tests/run_logic_tests.py
platformio test -d power-module -e native
python scripts/verify_release.py
```

Build Power first, then Master. The [CI workflow](.github/workflows/firmware-ci.yml) builds both controllers, runs regression/native tests, and verifies releases. Passing software tests does not replace validation on the real boards, isolator, power supply, and relay outputs.

## Troubleshooting

### Repeated `Brownout detector was triggered`

The Master supply or wiring cannot maintain voltage during startup. Use a stable 5 V source, shorten wiring, check grounds, and do not depend on a weak programmer for board power.

### Master boots but Ethernet does not start

Confirm the classic WT32-ETH01 board, use the merged image for a clean install, verify 5 V power, cable, and switch port, then inspect the 115200-baud console.

### Power Module remains offline

Verify 9600 baud, 8N1; cross TX/RX; confirm GPIO33 → Power RX and GPIO32 ← Power TX; and check isolator power, directions, and enables. At low voltage, `PING` must return `PONG`.

### Terminal shows random characters

Use 115200 baud for the Master service console and 9600 baud for the runtime Power protocol.

### A matching rule does not RESET a device

Read its dashboard block reason. Possible causes include disabled automation/rule/device, insufficient consecutive cycles, stabilization, retry delay, hourly limit, global spacing, an active transaction, offline Power Module, or an invalid channel assignment.

### Settings do not save

Confirm authentication, changed default password, valid required fields, and connectivity. After changing network settings, reconnect at the new address. Use the service console and browser developer tools to distinguish validation errors from connection loss.

## Documentation

```text
master/          WT32-ETH01 firmware, tests, docs, and releases
power-module/    SONOFF 4CHR3 firmware, tests, docs, and releases
scripts/         release verification utilities
.github/         continuous-integration workflow
```

English references:

- [Master technical documentation](master/docs/DOCUMENTATION-EN.md)
- [Master user manual](master/docs/USER-MANUAL-EN.md)
- [Logic model](master/docs/LOGIC-MODEL-0.9-EN.md)
- [Functional coverage](master/docs/FUNCTIONAL-COVERAGE-0.9-EN.md)
- [Production checklist](master/docs/PRODUCTION-CHECKLIST-EN.md)
- [Master release notes](master/docs/RELEASE-NOTES-0.9.0-RC6-EN.md)
- [Home Assistant integration](master/docs/HOME-ASSISTANT-EN.md)
- [Power technical documentation](power-module/docs/DOCUMENTATION-EN.md)
- [Power user manual](power-module/docs/USER-MANUAL-EN.md)
- [Power flashing guide](power-module/docs/FLASHING-EN.md)
- [UART protocol](power-module/docs/UART-PROTOCOL-EN.md)
- [Hardware validation](power-module/docs/HARDWARE-VALIDATION-EN.md)
- [Power test report](power-module/docs/TEST-REPORT-EN.md)

## License

MarkLabs.pl NetGuard is proprietary software. Source availability does not grant permission to copy, modify, redistribute, sublicense, manufacture, or commercially deploy it except as expressly permitted in [LICENSE](LICENSE).

Report security issues through the process in [SECURITY.md](SECURITY.md), not through a public issue containing sensitive details.
