# MarkLabs NetGuard — user manual

Applies to firmware `0.9.0-rc.6` for WT32-ETH01 in `PRODUCTION` mode.

## 1. Purpose

NetGuard monitors selected network devices and services. Rules can request a RESET
of a device connected to one of four Power Module outputs. A diagnostic target is
observation-only; a powered device is a physical load mapped to CH1–CH4.

## 2. First start

1. Connect Ethernet and start the Master and Power Module.
2. Find the DHCP address in the router or on the Master console at 115200 baud.
3. Open `http://<ip-address>/`.
4. Sign in with username `admin` and initial password `admin`.
5. Set a private password on **Network**. It must contain 8–32 characters.

After five failed sign-ins, the panel blocks further attempts for 60 seconds.

The default configuration uses DHCP. **Internet A** (`1.1.1.1`) and **Internet B**
(`8.8.8.8`) are enabled PING targets. Powered devices and automation rules are
disabled by default.

## 3. Status screen

The main screen reports:

- IP address, uptime, memory and previous ESP32 reset reason,
- Ethernet state, MAC, speed, duplex and link changes,
- Power Module state, heartbeats, timeouts, invalid UART lines and command state,
- target results, response time, availability and check count,
- powered devices, rules, condition progress and recent decisions,
- remaining stabilization and retry-delay time.

**Run test** starts the next cycle early; it does not interrupt an active cycle.
**Pause** prevents automatic RESET actions while measurements continue. **Manual
RESET** requires an enabled device, an idle Power Module and at least three consecutive
heartbeats.

## 4. Network and system settings

The **Network** page provides:

- **DHCP** — automatic addressing; static values remain stored but unused,
- **IPv4, subnet, gateway and DNS** — used when DHCP is disabled,
- **diagnostic interval** — 5–300 seconds, default 15 seconds,
- **automation** — global permission for rule actions,
- **startup protection** — 0–3600 seconds, default 120 seconds,
- panel password — leave blank to retain the current password.

DHCP or static-network changes apply after **Restart / Uruchom ponownie**. Restart
is rejected while a RESET is in progress.

## 5. Powered devices

The **Devices & logic** page contains four device slots. For each device configure:

- name, icon and a unique CH1–CH4 output,
- **RESET (1–300 s)** — power-off duration,
- **Stabilization (0–3600 s)** — startup time after a confirmed RESET,
- **Retry delay (0–86400 s)** — minimum time from a confirmed RESET until another
  RESET of the same device is allowed,
- **Limit/h (0–20)** — confirmed RESET limit in a one-hour window; `0` disables it.

Two enabled devices cannot share the same output.

## 6. Diagnostic targets

Up to 12 targets may be configured independently of powered devices.

- **PING** resolves a hostname when needed and sends two ICMP probes.
- **TCP** attempts to open a port; port `0` selects port 80.
- **HTTP** sends `HEAD` and requires the configured status code; port `0` selects 80.
- **HTTPS** behaves like HTTP and selects port 443 by default. 0.9.0-rc.6 does not verify
  the server certificate, so the test confirms availability rather than identity.
- **DNS** checks whether the hostname resolves to an IP address.

Timeout range is 100–10000 ms. HTTP/HTTPS paths must start with `/`, and expected
status is 100–599. Enter an IPv4 address or hostname only—do not include `http://`,
a port or path in the address field.

## 7. Automation rules

Up to eight rules are available. A rule uses 1–4 conditions and either:

- **ALL (AND)** — every used condition must match,
- **ANY (OR)** — one matching condition is sufficient.

A condition selects a target and expected **UP** or **DOWN** state. **Consecutive
cycles (1–10)** controls how many results must match before RESET is attempted. An
enabled rule must reference enabled targets and an enabled powered device.

Example: `AP is DOWN` AND `router is UP` AND `Internet A is UP` → `RESET AP`. This
reduces unnecessary AP resets during a complete network outage.

## 8. Safeguard order

An automatic RESET is allowed only after the rule matches for the required cycles and:

- automation is enabled and Ethernet has an IP address,
- startup protection has elapsed,
- the Power Module is online with three consecutive heartbeats,
- stabilization and retry delay have elapsed,
- the global 15-second spacing after the last confirmed RESET has elapsed,
- the hourly limit is available and no other command is active.

Only one RESET is processed at a time. Success is recorded only after the UART event
confirming that the channel has switched back on. Manual RESET bypasses rule
conditions only; every timing safeguard and the hourly limit still apply.

## 9. Saving configuration

**Save complete logic** stores devices, targets and rules together. A successful
message includes a revision number. Saving is rejected during RESET or for invalid
data, such as duplicate CH assignments or a rule referencing a disabled target. Do
not leave the page before the result appears.

## 10. Configuration backup

**Backup** exports networking, automation, devices, targets and rules to JSON. The
panel password is excluded. Restore accepts only `NETGUARD-MASTER` format 1 backups.
Automation is disabled before import; after an error it remains disabled for review.
Restart the unit after restoring different network settings.

## 11. Home Assistant

On **Home Assistant**, enter the MQTT broker, port, optional credentials, device name
and prefixes. Defaults are `homeassistant` and `marklabs/netguard`. After enabling the
integration, MQTT Discovery adds the device and entities automatically.

Home Assistant receives Ethernet, Power Module, system and target sensors, RESET/test
buttons and the automation switch. The MQTT password stays in NVS and is excluded
from read APIs and backups. See `HOME-ASSISTANT-EN.md` for complete setup.

## 12. OTA update

1. Obtain the WT32-ETH01 image ending in `-app.bin`.
2. Open **OTA**, select the file and start the update.
3. Keep power and Ethernet connected until writing completes.
4. The unit restarts after a successful update.

OTA is rejected during RESET. Firmware checks the filename, ESP32 header, available
space and write result. 0.9.0-rc.6 does not provide cryptographic signing or automatic
rollback.

## 13. Troubleshooting

- **Panel unavailable:** check Ethernet link, DHCP and the 115200-baud console.
- **Sign-in rejected:** wait 60 seconds after repeated failures.
- **Power Module offline:** check 9600-baud UART, RX/TX, isolator and `PONG` heartbeat.
- **Rule does not RESET:** read its state; it may be waiting for cycles, startup,
  Power Module, stabilization, retry delay, hourly limit or global spacing.
- **Save rejected:** read the error, remove duplicate CH mappings and validate every
  enabled target and rule.
- **Panel unavailable after IP change:** restart and open the new address.
- **MQTT disconnected:** verify address, port, credentials, TLS mode and broker
  reachability; the state code is available through the status endpoint and `MQTT`
  console command.

The Master console uses 115200 baud. Power Module UART uses 9600 baud on GPIO32 (RX)
and GPIO33 (TX). Master 0.9.0-rc.6 uses the transactional
`RESET <SESSION> <ID> <CH> <duration>` command from NETGUARD-POWER 0.3.1. Install Power 0.3.1 first, followed by Master 0.9.0-rc.6.
