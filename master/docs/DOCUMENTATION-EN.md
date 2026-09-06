# MarkLabs NetGuard — firmware documentation

Release candidate: `0.9.0-rc.6`, hardware: WT32-ETH01, mode: `PRODUCTION`.

## Architecture

The firmware has one active diagnostics and automation path. `LogicEngine` tests
targets, evaluates rules and is the only component allowed to request a `RESET` from
the Power Module.

## Web panel

- `/` — Ethernet, UART, device, target, rule and latest-decision status,
- `/config` — DHCP/static IPv4, test interval and password,
- `/logic` — four powered devices, diagnostic targets and rules,
- `/home-assistant` — MQTT broker, Discovery, TLS and connection state,
- `/backup` — complete configuration export and controlled restore,
- `/update` — OTA update using an `*-app.bin` file.

On the main screen, **Run test** requests a new cycle without waiting for the next
interval. **Pause** blocks new RESET actions while diagnostics continue. Pressing it
again resumes automation.

Each enabled device card contains **Manual RESET**. It uses the device's configured
RESET duration and is accepted only when the Power Module is online, has confirmed
three consecutive heartbeats and is not processing another command. A manual action
bypasses rule conditions only; startup protection, stabilization, global spacing,
retry delay and the hourly limit still apply. It completes only after the UART
`CHANNEL_ON` event and is recorded in the event log.

Saving devices, targets or rules is temporarily blocked while a RESET is in progress.
This prevents CH mapping from changing between command transmission and acknowledgement.

The login is `admin`. The initial password for a new configuration is `admin`.

Status also reports the previous ESP32 reset reason, Ethernet speed and duplex,
consecutive valid heartbeats and whether the UART is processing a command.
The dashboard also shows MQTT connection state.

## Home Assistant

The optional MQTT module uses Discovery, retained state and Last Will. It publishes
Ethernet, Power Module, system and enabled-target state plus RESET counters. It
provides an automation switch, test button and RESET buttons for enabled devices.
RESET commands pass through `LogicEngine` and never control outputs directly.

Configuration uses a separate CRC-protected NVS namespace. TLS encrypts the
connection, but this release does not verify the broker certificate. MQTT is disabled
by default. See `HOME-ASSISTANT-EN.md` for setup details.

## Operating model

A powered device is a physical load assigned uniquely to CH1–CH4. Two enabled
devices cannot use the same output.

A diagnostic target is an independent address observed with PING, TCP, HTTP, HTTPS
or DNS. It is not implicitly tied to an outlet and may be reused in multiple rules.

A rule combines one to four conditions with ALL (AND) or ANY (OR). It may request a
RESET only after the configured number of consecutive matching cycles.

## Execution safeguards

RESET remains blocked until:

- Ethernet has an active IP address,
- the configured startup protection time has elapsed (two minutes by default),
- the Power Module has confirmed at least three consecutive heartbeats,
- device stabilization has ended,
- retry delay and the hourly limit allow execution,
- the global spacing between recoveries has elapsed.

A model change during measurement invalidates the complete cycle. Consecutive-match
counters reset after configuration changes and network loss, preventing stale data
from triggering a new rule. A RESET counts as successful only after UART confirmation.

The panel exposes a separate state for every rule: watching, consecutive-cycle count,
startup delay, Power Module unavailable, stabilization, global spacing, retry delay,
hourly limit, Power Module busy or RESET requested. Each device reports remaining
stabilization and retry delay time and confirmed recoveries in the current hour. The
main log combines the latest 16 automation events with 24 current-session system
events: boot, Ethernet, Power Module, automation and OTA transitions.

The global automation switch and startup protection are also available in system
settings. The protection range is 0–3600 seconds. A value of `0` is intended only
for controlled tests; at least 120 seconds is recommended for normal operation.

## UART

The Master uses 9600 baud on GPIO32 (RX) and GPIO33 (TX). It monitors heartbeat while
`PING`/`PONG` remains a fallback. A power action uses
`RESET <Master session> <transaction> <channel 1-4> <seconds 1-300>` and requires matching identifiers. `OK RESET` acknowledges acceptance; only `EVENT CHANNEL_ON` completes the operation and starts stabilization.
The module reports its own session, restarts and logical CH1–CH4 states.

## Applying settings

Target and rule changes apply without rebooting. DHCP/static IPv4 changes apply after
a device restart. A restart action is available directly below the settings form.
System settings and the logic model are stored independently using
alternating CRC-protected NVS records.

## Configuration backup and OTA

The JSON backup contains networking, Home Assistant and automation settings plus every
device, target and rule. It deliberately excludes panel and MQTT passwords. Restore accepts only product
`NETGUARD-MASTER`, format 1 and the expected model sizes. It disables automation first,
writes and validates the model, and restores the requested automation state last. If
any step fails, automation remains disabled for operator review.

OTA requires authentication and a same-panel request, accepts an `*-app.bin` file,
checks the ESP32 image header, available partition space and the Update library's
validation result. All manual and automatic RESET actions are inhibited during the
write. Cryptographic image signing and automatic rollback are not included yet.

## Save diagnostics

A successful logic write returns a JSON `sequence` and shows its revision in the
panel. The 115200 console reports `LOGIC SAVE OK SEQUENCE=<n>` or `LOGIC SAVE FAIL
<code>`. System settings similarly report `SYSTEM CONFIG SAVE ...`. The sequence only
increments after writing and reading back a valid NVS record.
