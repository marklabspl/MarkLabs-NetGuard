# MarkLabs.pl NetGuard

**ESP32 Ethernet network watchdog and automatic four-channel power-recovery controller for routers, switches, access points, servers, cameras, and other network infrastructure.**

[![Firmware CI](https://github.com/marklabspl/MarkLabs-NetGuard/actions/workflows/firmware-ci.yml/badge.svg)](https://github.com/marklabspl/MarkLabs-NetGuard/actions/workflows/firmware-ci.yml)
[![Pre-release](https://img.shields.io/github/v/release/marklabspl/MarkLabs-NetGuard?include_prereleases&label=pre-release)](https://github.com/marklabspl/MarkLabs-NetGuard/releases)
[![Master](https://img.shields.io/badge/master-WT32--ETH01-2196f3)](master/)
[![Power module](https://img.shields.io/badge/power-SONOFF%204CHR3-00bcd4)](power-module/)
[![License](https://img.shields.io/badge/license-proprietary-64748b)](LICENSE)

MarkLabs.pl NetGuard is a self-contained network monitoring and controlled power-recovery appliance for unattended LAN infrastructure. It checks configurable IP addresses and services, evaluates dependency-aware recovery rules, and can safely request a timed power cycle on one of four connected power channels when the configured evidence indicates a genuine failure.

The system consists of two controllers:

- **NetGuard Master** — classic WT32-ETH01 (ESP32 + LAN8720): Ethernet diagnostics, automation logic, web UI, configuration, event logs, backup/restore, OTA, and Home Assistant MQTT integration.
- **NetGuard Power** — SONOFF 4CHR3 with ESP8285: four relay channels, local reset timers, heartbeat/status telemetry, and acknowledged RESET execution.

The two controllers communicate over an **isolated wired UART link**. Network diagnostics and recovery decisions remain on the Master, while relay timing remains local to the Power Module. This separation allows an already-started RESET to finish even if communication with the Master is interrupted.

> [!IMPORTANT]
> NetGuard is currently **pre-release software**. The hardware paths and core recovery functions are already implemented and tested on real hardware, but configuration details, protocol internals, web UI elements, release filenames, and documentation may still change before the first stable release.

> [!IMPORTANT]
> Always use a **compatible Master and Power Module release pair**. Do not mix protocol generations. Check the current GitHub Release notes and bundled documentation before updating either controller.

---

## MarkLabs NetGuard article series

The complete English build series documents the project from architecture and hardware modification through Master commissioning, Home Assistant integration, and real failure testing:

1. [Part 1 — NetGuard: Building a Wired Network Watchdog](https://marklabs.pl/en/netguard-network-watchdog/)
2. [Part 2 — NetGuard Power: Turning a SONOFF 4CHR3 into a Four-Channel Recovery Module](https://marklabs.pl/en/netguard-power-sonoff-4chr3/)
3. [Part 3 — NetGuard Master: WT32-ETH01, Ethernet and Intelligent Fault Detection](https://marklabs.pl/en/netguard-master-wt32-eth01/)
4. [Part 4 — NetGuard and Home Assistant: MQTT, Monitoring and Automatic Recovery](https://marklabs.pl/en/netguard-home-assistant-mqtt/)
5. [Part 5 — NetGuard in Practice: Installation and Network Failure Testing](https://marklabs.pl/en/netguard-network-watchdog-testing/)

Main project page: [MarkLabs.pl](https://marklabs.pl/)

---

## Contents

- [Project status](#project-status)
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
- [License](#license)

---

## Project status

NetGuard is actively developed as a **pre-release** project.

The following core paths have already been exercised on real hardware:

- WT32-ETH01 Master serial flashing and normal boot;
- wired Ethernet operation on the classic WT32-ETH01;
- isolated Master ↔ Power UART communication;
- Power Module heartbeat and ONLINE/OFFLINE supervision;
- all four SONOFF 4CHR3 relay channels;
- controlled timed RESET and automatic return to ON;
- recovery completion even when UART is interrupted after RESET has started;
- diagnostic targets and dependency-aware rules;
- recovery timing and anti-loop safeguards;
- full detection → RESET → stabilization → re-test flow.

Pre-release does **not** mean that these paths are only theoretical. It means the project is still evolving before the first stable public interface and configuration model are frozen.

---

## Features

The current pre-release implementation includes:

- Four independently named and configured powered-device channels.
- Up to 12 reusable diagnostic targets, separate from powered devices.
- PING, TCP, HTTP, HTTPS, and DNS diagnostics.
- Up to 8 rules with 1–4 conditions each.
- `ALL` / `ANY` matching.
- Expected `UP` / `DOWN` state per condition.
- 1–10 consecutive-cycle confirmation before a rule may trigger.
- Controlled RESET transactions with local power-off timing.
- Startup protection, stabilization, retry-delay, hourly-limit, and global-spacing safeguards.
- Responsive English/Polish web panel.
- Live system, Ethernet, target, powered-device, rule, UART, and recovery status.
- DHCP or static IPv4 addressing.
- Configuration backup and restore.
- Master web OTA using the application image intended for OTA.
- Home Assistant integration through MQTT Discovery.
- Persistent configuration and recovery counters.
- Versioned and acknowledged UART protocol.
- Prebuilt release binaries with SHA-256 manifests.

Because the project is pre-release, limits and UI details may change. The documentation bundled with the current release is authoritative.

---

## Architecture and supported hardware

```text
                Ethernet / management LAN
                         |
                 +-------+--------+
                 | NetGuard Master |
                 | WT32-ETH01      |
                 | diagnostics     |
                 | rules / web     |
                 | MQTT / recovery |
                 +-------+--------+
                         |
                  isolated UART
                   9600 baud, 8N1
                         |
                 +-------+--------+
                 | NetGuard Power  |
                 | SONOFF 4CHR3    |
                 | CH1 CH2 CH3 CH4 |
                 +--+---+---+---+--+
                    |   |   |   |
                 powered network devices
```

### Master hardware

The supported Master board is the classic **WT32-ETH01** based on ESP32 and LAN8720 Ethernet.

It is **not** the similarly named ESP32-C3 Ethernet board and should not be replaced with an arbitrary ESP32 + RJ45 module without verifying pin mapping, Ethernet PHY configuration, boot behaviour, flash layout, and firmware support.

For the NetGuard reference build, a **stable 5 V supply is recommended** because it provides good startup margin for the ESP32 and Ethernet PHY.

The WT32-ETH01 hardware itself can be powered from either:

- the `5V` supply input, **or**
- the `3V3` supply input.

Choose **one supply method only**. Never power both rails at the same time.

Initial flashing and service access require a **3.3 V logic-level USB-to-UART adapter**.

### Power Module hardware

The Power Module target is the **SONOFF 4CHR3 with ESP8285**.

Stock SONOFF firmware is not compatible with the NetGuard protocol. The module must run the matching NetGuard Power firmware.

The Power Module has no role in network diagnosis or rule evaluation. Its job is to execute an acknowledged, bounded power interruption and restore the selected channel locally.

---

## Safety and security

NetGuard Power controls equipment connected to hazardous mains voltage.

Firmware installation, enclosure work, mains wiring, isolation, grounding, protection, and production deployment must be performed by a qualified person and must comply with applicable electrical regulations.

### Mains safety

- Fully disconnect the Power Module from mains before connecting a programmer or touching the PCB.
- Never power the SONOFF from mains and a USB-to-UART programmer at the same time.
- Use only the documented low-voltage programming method.
- Use galvanic isolation between the Master UART and the mains-powered relay controller.
- Power the two sides of the digital isolator according to the isolator/module requirements.
- Do not bridge the isolated ground domains unless the specific isolator design explicitly requires it.
- Perform initial firmware, UART, and relay-control tests in a controlled environment before connecting production loads.

### Network security

NetGuard is intended for a **trusted management network**.

Current pre-release security limitations must be understood before deployment:

- the web panel uses HTTP Basic Authentication over HTTP;
- HTTPS diagnostic checks do not validate the remote server certificate;
- MQTT TLS currently uses insecure certificate validation mode;
- OTA images are not cryptographically signed.

Therefore:

- do **not** expose NetGuard directly to the Internet;
- place it on a protected management LAN/VLAN;
- use firewall rules appropriate for your network;
- use VPN for remote administration;
- use a dedicated MQTT account restricted to the NetGuard topic namespace;
- change the default panel credentials immediately after the first login;
- install only trusted, checksum-verified firmware images.

See [SECURITY.md](SECURITY.md) for the current security model and reporting process.

---

## Hardware connections

There are **two different UART interfaces** on the Master. Do not confuse them:

1. **Service UART** — used for flashing, boot logs, and diagnostics.
2. **Runtime Power UART** — dedicated to communication with NetGuard Power through the isolator.

They use different pins and different baud rates.

### Master service UART — flashing and console

Use a **3.3 V logic-level** USB-to-UART adapter.

For the reference build, power the WT32-ETH01 from a separate stable 5 V source while using the adapter only for TX/RX/GND.

| USB-to-UART | WT32-ETH01 |
|---|---|
| TX | RX0 / GPIO3 |
| RX | TX0 / GPIO1 |
| GND | GND |

Do not feed 5 V logic into the UART pins.

To enter the ESP32 ROM bootloader:

1. pull `IO0` low;
2. reset or power-cycle the board;
3. once the ROM bootloader has started, release `IO0`;
4. flash the image;
5. remove the `IO0` → GND connection before normal boot.

The Master service console uses:

```text
115200 baud
8 data bits
no parity
1 stop bit
```

or simply:

```text
115200 8N1
```

### Master runtime UART — connection to NetGuard Power

NetGuard uses a separate UART path for normal Master ↔ Power communication.

| WT32-ETH01 | Direction | Power Module |
|---|---:|---|
| GPIO33 (TX) | Master → Power | RX through isolator |
| GPIO32 (RX) | Power → Master | TX through isolator |
| local reference | isolator-specific | isolated reference |

Runtime communication uses:

```text
9600 baud, 8N1
```

TX and RX are crossed across the communication path.

> [!NOTE]
> On WT32-ETH01 documentation/silkscreen, GPIO32 and GPIO33 may also be associated with board-specific labels/functions. NetGuard firmware intentionally uses the documented project mapping for its runtime Power UART. Follow the NetGuard wiring documentation for the supported firmware build rather than assuming a generic WT32 peripheral assignment.

### ADuM1201 isolation

The reference NetGuard design uses an **ADuM1201-based digital isolator** between Master and Power.

The two sides must be powered according to the actual isolator module being used. Verify:

- side A supply voltage;
- side B supply voltage;
- GND/reference for each side;
- signal direction for both isolator channels;
- enable pins, if present;
- that Master TX reaches Power RX;
- that Power TX reaches Master RX.

Do not assume that two visually similar ADuM1201 breakout boards have identical pin order.

### Power Module programming UART

With mains fully disconnected:

| USB-to-UART | SONOFF 4CHR3 |
|---|---|
| TX | RX |
| RX | TX |
| GND | GND |
| regulated 3.3 V | 3.3 V |

Hold `IO0` low while applying 3.3 V to enter the bootloader.

Never apply 5 V to the SONOFF 3.3 V rail or UART pins.

---

## Firmware installation

### Download a matched release pair

Do not copy firmware filenames from an old README or article.

Download the current compatible Master and Power files from:

- [GitHub Releases](https://github.com/marklabspl/MarkLabs-NetGuard/releases)

Use the release notes and included documentation to confirm that the selected Master and Power Module builds use the same protocol generation.

### Verify SHA-256 before flashing

Release packages include SHA-256 manifests.

Example on PowerShell:

```powershell
Get-FileHash .\firmware-file.bin -Algorithm SHA256
```

Compare the result with the checksum published for that release.

### Master image types

Master releases may contain at least two different image types:

- `*-merged.bin` — **full serial installation image**, including the expected bootloader/partition layout; use this for a blank device or clean serial installation at `0x0000`.
- `*-app.bin` — **application-only image** intended for web OTA or for a board that already has a known-compatible bootloader and partition layout.

> [!WARNING]
> Do not flash the application-only image at `0x0000` on a blank WT32-ETH01. It is not a complete replacement for the merged serial image.

### Required controller update order

When updating both controllers:

1. Disable global Master automation.
2. Update/flash **NetGuard Power first**.
3. Update/flash **NetGuard Master immediately afterward**.
4. Confirm protocol compatibility.
5. Confirm Power Module ONLINE state and stable heartbeat reception.
6. Perform one controlled manual RESET.
7. Re-enable automation only after both controllers have been verified.

Never leave automatic recovery enabled while the two controllers run incompatible protocol generations.

### Install the Power Module

1. Fully disconnect mains and verify that the board is not energized.
2. Connect the documented 3.3 V programmer.
3. Hold `IO0` low and apply 3.3 V.
4. Flash the current NetGuard Power serial image at `0x00000`.
5. Remove power.
6. Disconnect the programmer.
7. Release `IO0`.
8. At safe low voltage, open a 9600-baud 8N1 terminal.
9. Send `PING` followed by Enter.
10. Confirm the response:

```text
PONG
```

See the Power Module flashing guide in `power-module/docs/` for the current release procedure.

### Install the Master

1. Connect the Master service UART.
2. Power the WT32-ETH01 from a stable supply.
3. Hold `IO0` low while resetting or powering the board.
4. Release `IO0` once the ROM bootloader has started.
5. Flash the current **merged Master image** at `0x0000`.
6. Remove the `IO0` → GND connection.
7. Reset the WT32-ETH01 normally.
8. Open the service console at 115200 baud, 8N1.
9. Confirm that the NetGuard Master boot banner appears and that normal firmware starts.

Example `esptool` command:

```bash
esptool.py --chip esp32 --port COM5 --baud 460800 write_flash 0x0000 netguard-master-wt32-eth01-merged.bin
```

Replace `COM5` and the filename with the actual values from your current release package.

---

## First start

Do not enable automatic recovery immediately after flashing.

Commission the system in stages.

### 1. Start the Master alone

1. Remove the `IO0` bootloader connection.
2. Connect Ethernet.
3. Power on the WT32-ETH01.
4. Monitor the 115200-baud service console.
5. Confirm normal boot and Ethernet initialization.

### 2. Find the Master IP address

DHCP is enabled by default.

Find the assigned address using either:

- the Master service console; or
- your DHCP server/router lease table.

Then open:

```text
http://<device-ip>/
```

### 3. Log in and change the default password

On a new installation, use the credentials documented for the current release.

If the current pre-release still uses the default `admin` / `admin` credentials, change the password immediately before doing anything else.

Use a unique password of at least 8 characters.

### 4. Keep automation disabled

Before automatic recovery is enabled:

- verify Ethernet status;
- verify the configured IP mode;
- verify system time/uptime behaviour;
- define diagnostic targets;
- define powered devices;
- define rules;
- connect and verify the Power Module;
- perform manual diagnostics;
- perform one controlled manual RESET.

Only then enable tested rules and global automation.

### Static IPv4

Static mode requires a valid:

- IP address;
- subnet mask;
- gateway;
- DNS configuration.

After changing network addressing, reconnect to the new address.

If the panel becomes unreachable, use the **115200-baud service UART** as the primary diagnostic path.

---

## Operating principle

NetGuard deliberately separates:

- **what is monitored**, from
- **what is powered**.

A **diagnostic target** is an address or service to check.

A **powered device** is a physical recovery endpoint mapped to one of the four SONOFF relay channels.

A **rule** connects diagnostic evidence to a recovery action.

This makes configurations possible where the monitored target and the device being power-cycled are not the same hardware.

Example:

> If an access point is DOWN while the main router and a reference DNS target are UP, RESET the powered device assigned to that access point.

Another example:

> If a PoE camera is DOWN while the rest of the LAN is healthy, a rule may RESET the powered PoE switch rather than the camera itself.

Be careful: restarting a PoE switch affects every device powered by that switch.

For every diagnostic cycle, the Master:

1. Runs enabled target checks.
2. Records state, latency, errors, and counters.
3. Evaluates enabled rules against that coherent cycle.
4. Requires the configured number of consecutive matching cycles.
5. Checks automation, device, rule, timing, limit, UART, and channel safeguards.
6. Starts a RESET transaction with unique session/transaction context.
7. Waits for the Power Module acknowledgement.
8. Waits for the final channel-restored event.
9. Records the result.
10. Updates persistent recovery counters.
11. Starts stabilization and retry timing.

A command being transmitted is **not** the same thing as successful recovery.

An acknowledgement confirms that the Power Module accepted the transaction.

Recovery is only considered complete after the expected final event confirms that the channel has been returned to ON.

---

## Configuration model

### Diagnostic targets

The current pre-release supports up to **12 independent diagnostic targets**.

Targets are independent of the four physical power channels.

A target can include:

- name;
- icon;
- address/host;
- diagnostic method;
- timing;
- method-specific parameters.

Supported methods:

- **PING** — ICMP reachability and latency.
- **TCP** — connection to a selected TCP port.
- **HTTP** — HTTP request and response evaluation.
- **HTTPS** — HTTPS request; current pre-release does not validate the remote certificate.
- **DNS** — lookup using a configured DNS server.

Do not assume that a single failed PING is enough evidence for recovery.

### Powered devices

Up to **four** powered devices map to physical channels `CH1`–`CH4`.

Device configuration may include:

- name;
- icon;
- purpose;
- power-off time;
- stabilization time;
- retry delay;
- recovery limits;
- channel assignment;
- enabled/disabled state.

One physical channel must not be assigned to multiple powered devices at the same time.

### Rules

The current pre-release supports up to **8 rules**.

Each rule contains:

- one powered device to RESET;
- 1–4 diagnostic conditions;
- expected `UP` or `DOWN` state per condition;
- `ALL` mode or `ANY` mode;
- required consecutive matching cycles;
- enabled/disabled state.

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

Positive upstream/reference conditions help prevent pointless local restarts when the router, LAN, or upstream service is the actual cause of the outage.

### Manual RESET

A manual RESET bypasses the diagnostic rule conditions only.

It must still obey the safety and transaction layer, including:

- valid channel assignment;
- Power Module availability;
- valid runtime UART;
- transaction protection;
- allowed reset timing;
- active-operation protection;
- applicable recovery safeguards implemented by the current release.

---

## Recovery safeguards

NetGuard is designed around the idea that a power cycle should be a **controlled recovery action**, not the first reaction to one failed packet.

Typical safeguards include:

| Setting | Typical supported range | Purpose |
|---|---:|---|
| Power-off time | 1–300 s | How long the selected channel remains OFF. |
| Stabilization | 0–3600 s | Time for the recovered device to boot before new decisions. |
| Retry delay | 0–86400 s | Minimum delay before another automatic recovery attempt. |
| Hourly limit | release-defined | Limits repeated automatic recovery. |
| Global spacing | release-defined | Prevents recovery actions from starting too close together system-wide. |

The engine also protects against situations such as:

- duplicate channel assignments;
- overlapping RESET operations;
- stale UART transactions;
- automatic action while global automation is disabled;
- action while the selected rule or device is disabled;
- immediate re-triggering while a device is still stabilizing;
- repeated recovery loops.

Persistent recovery counters and history allow the Master to retain relevant operational context across normal restarts.

---

## Web UI, backup, and OTA

The bilingual web dashboard provides visibility into:

- Master firmware and uptime;
- Ethernet link/addressing;
- Power Module UART and heartbeat health;
- target state and latency;
- powered-device state;
- rule conditions;
- rule block reasons;
- active recovery state;
- stabilization/retry timing;
- counters;
- recent events.

### Configuration storage

Configuration is validated before storage.

The current implementation uses redundant persistent records with integrity validation so that a partial/corrupt configuration write is less likely to be accepted as valid configuration.

### Backup and restore

Backups contain operational configuration but intentionally exclude sensitive panel/MQTT passwords.

Store credentials separately.

Restore should keep automation disabled while configuration is being validated and applied.

### OTA

Master OTA uses the **application image**, not the full merged serial image.

Use only the `*-app.bin` image documented for OTA by the current release.

Do not remove power during OTA writing.

The current pre-release has no cryptographic OTA signature verification and no automatic rollback, so install only trusted, SHA-256-verified images.

For a blank board or clean serial installation, use the **merged serial image** instead.

---

## Home Assistant

Home Assistant is an **optional integration layer**, not a dependency of the NetGuard recovery engine.

The Master can operate its Ethernet diagnostics, rule engine, recovery safeguards, and Power Module link without Home Assistant and without MQTT.

The Master publishes MQTT Discovery entities for supported:

- system state;
- diagnostic targets;
- powered devices;
- rules;
- recovery status;
- controls exposed by the current firmware.

Recommended MQTT deployment:

- use a dedicated MQTT user;
- restrict it to the NetGuard topic namespace;
- keep the broker on a trusted network;
- configure broker address, credentials, base topic, and optional TLS in the Master panel;
- verify entity availability before enabling remote controls.

The design goal is simple:

> If the Home Assistant server is down, NetGuard must still be able to detect the failure and perform the configured recovery independently.

See the Home Assistant documentation in `master/docs/` for the current entity set and configuration procedure.

---

## UART protocol

NetGuard uses a line-oriented ASCII runtime protocol between Master and Power.

The exact protocol generation is versioned and may change during pre-release development.

Current protocol characteristics include:

- 9600 baud;
- 8N1;
- ASCII lines;
- CR, LF, or CRLF line endings;
- acknowledged recovery commands;
- Master boot-session identity;
- transaction identity;
- periodic Power Module heartbeat;
- explicit channel-restored event.

Conceptually:

```text
Master -> Power: PING
Power  -> Master: PONG
```

A recovery transaction follows the protocol format documented by the **current compatible release pair**.

Do not copy an old RESET command format from an earlier article, release, or README. During pre-release development the protocol has evolved, so the UART specification bundled with the active release is authoritative.

A successful transaction must distinguish between:

1. command transmitted;
2. command acknowledged;
3. power channel restored.

The final channel-restored event is what confirms completion of the power cycle.

See the current UART specification in `power-module/docs/`.

---

## Building and testing

Requirements include:

- PlatformIO Core;
- Python 3;
- toolchains installed by the declared PlatformIO environments.

Typical build/test workflow:

```bash
git clone https://github.com/marklabspl/MarkLabs-NetGuard.git
cd MarkLabs-NetGuard

platformio run -d power-module -e sonoff_4chr3
platformio run -d master -e wt32_eth01

python master/tests/run_regression_tests.py
python power-module/tests/run_logic_tests.py
platformio test -d power-module -e native
python scripts/verify_release.py
```

Build and update **Power first, then Master** when protocol compatibility requires both sides to move together.

The CI workflow builds both controllers, runs the repository tests, and verifies release artifacts.

Passing software tests does **not** replace physical validation on:

- the real WT32-ETH01;
- the real SONOFF 4CHR3;
- the actual ADuM1201 isolator module;
- the chosen Master power supply;
- Ethernet cabling/switching;
- relay outputs;
- the final mains enclosure and wiring.

---

## Troubleshooting

### Repeated `Brownout detector was triggered`

The Master supply or wiring cannot maintain adequate voltage during startup.

Check:

- supply quality;
- cable/wire length;
- voltage drop;
- ground connection;
- connector quality.

For the reference build, use a stable 5 V source rather than relying on a weak USB-to-UART adapter to power the WT32-ETH01 and Ethernet PHY.

### Master boots but Ethernet does not start

Check:

1. that the board is the classic WT32-ETH01;
2. that the correct Master firmware target was used;
3. that a full/merged image was used for a clean installation;
4. that `IO0` is no longer held low after flashing;
5. the power supply;
6. Ethernet cable;
7. switch/router port;
8. the 115200-baud Master service console.

### Master keeps entering the bootloader

Remove the `IO0` → GND connection used for flashing, then reset the board normally.

### Power Module remains OFFLINE

Check:

- 9600 baud, 8N1;
- runtime Master UART pins;
- crossed TX/RX;
- GPIO33 Master TX path;
- GPIO32 Master RX path;
- isolator supplies;
- isolator signal direction;
- enable pins, if present;
- isolated-side references.

At safe low voltage, verify that the Power Module itself responds:

```text
PING
PONG
```

### Terminal shows random characters

Make sure you are using the correct UART and baud rate:

- **115200** — Master service console;
- **9600** — Master ↔ Power runtime protocol.

These are different interfaces.

### A matching rule does not RESET a device

Check the dashboard block reason.

Possible causes include:

- global automation disabled;
- rule disabled;
- powered device disabled;
- insufficient consecutive matching cycles;
- stabilization active;
- retry delay active;
- recovery limit reached;
- global spacing active;
- another transaction active;
- Power Module offline;
- invalid or conflicting channel assignment.

### Settings do not save

Check:

- panel authentication;
- whether the default password has been changed if required by the current release;
- required fields;
- network connectivity;
- validation messages.

After changing network settings, remember to reconnect using the new address.

Use the service UART and browser developer tools to distinguish a validation error from simple loss of network connectivity.

---

## Documentation

Repository layout:

```text
master/          WT32-ETH01 firmware, tests, docs, and release files
power-module/    SONOFF 4CHR3 firmware, tests, docs, and release files
scripts/         release verification utilities
.github/         continuous-integration workflow
```

English documentation is maintained inside the repository, including:

- Master technical documentation;
- Master user manual;
- logic model;
- functional coverage;
- production checklist;
- Master release notes;
- Home Assistant integration guide;
- Power technical documentation;
- Power user manual;
- Power flashing guide;
- UART protocol specification;
- hardware validation;
- Power test report.

Use the documentation shipped with the current release when there is any difference between an older article and the active pre-release firmware.

---

## License

MarkLabs.pl NetGuard is source-available under the terms of the repository [LICENSE](LICENSE).

Source availability does not automatically grant permission to copy, modify, redistribute, sublicense, manufacture, or commercially deploy the project outside the permissions explicitly granted by that license.

Read [LICENSE](LICENSE) before reusing the code or design.

Report security issues through the process described in [SECURITY.md](SECURITY.md). Do not disclose sensitive vulnerabilities in a public issue.
