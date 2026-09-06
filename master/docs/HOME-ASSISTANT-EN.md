# Home Assistant integration

Applies to NetGuard `0.9.0-rc.6`. The integration uses MQTT Discovery and requires no
manual YAML entity definitions.

## Requirements

- an MQTT broker reachable from the NetGuard network,
- the MQTT integration configured in Home Assistant,
- a dedicated broker account for NetGuard is recommended.

## Configuration

1. Open **Home Assistant** in the NetGuard panel.
2. Enter the broker address without `mqtt://`, port, username and password.
3. Keep discovery prefix `homeassistant` unless it was changed in HA.
4. Set the topic prefix, default `marklabs/netguard`.
5. Select the entity-name and state-value language: **Polski** or **English**.
6. Enable and save the integration. Status should become **MQTT CONNECTED**.

Port 1883 is typical without TLS and 8883 with TLS. TLS encrypts traffic in this
release but does not verify the broker certificate. Do not use it over an untrusted
network without additional network protection.

## Entities

Discovery creates one MAC-identified device with:

- Ethernet and Power Module binary sensors,
- IP, uptime, free memory, diagnostic-cycle and RESET counters,
- state and response time for each enabled target,
- state of each enabled device and rule,
- result of the latest command sent by Home Assistant,
- automation switch,
- run-test button,
- RESET button for each enabled powered device.

RESET buttons never control CH directly. Commands pass through `LogicEngine` and
require an enabled device, ready Power Module, three heartbeats and no active command.
State is published every five seconds and retained by the broker.

NetGuard publishes `online/offline` availability using MQTT Last Will. Discovery
messages are retained and resent after `homeassistant/status = online`.

MQTT and panel passwords are excluded from backups. Enter the MQTT password again
when restoring a backup on another unit.

English device states include `READY`, `POWER OFFLINE`, `RESET PENDING`,
`STABILIZING`, `RETRY DELAY` and `HOURLY LIMIT`. Their Polish equivalents include
`GOTOWE`, `BRAK MODUŁU`, `RESET W TOKU`, `STABILIZACJA`, `PONOWNA PRÓBA` and
`LIMIT GODZINOWY`. Rule states are published in the selected language as well. A
language change preserves broker settings and makes NetGuard refresh MQTT Discovery.
