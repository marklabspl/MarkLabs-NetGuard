# MarkLabs NetGuard Master

Current release candidate: `0.9.0-rc.6` for WT32-ETH01.

The firmware provides Ethernet monitoring, isolated Power Module UART, four uniquely
mapped powered devices, twelve PING/TCP/HTTP/HTTPS/DNS targets, eight ALL/ANY rules,
guarded RESET execution, retry delay, stabilization, hourly limits, authenticated
PL/EN web UI, configuration backup, OTA and an event history.

`LogicEngine` is the only component allowed to request a power action.

## Build

```powershell
platformio run
```

Build output is stored under `.pio/build/wt32_eth01/`. Release binaries are copied to
`firmware/` only when a complete firmware package is closed.

## Documentation

- [Current documentation index](docs/README.md)
- [Dokumentacja PL](docs/DOCUMENTATION-PL.md)
- [Documentation EN](docs/DOCUMENTATION-EN.md)
- [Instrukcja obsługi PL](docs/USER-MANUAL-PL.md)
- [User manual EN](docs/USER-MANUAL-EN.md)
- [Model logiki PL](docs/LOGIC-MODEL-0.9-PL.md)
- [Logic model EN](docs/LOGIC-MODEL-0.9-EN.md)
- [Pokrycie funkcjonalne PL](docs/FUNCTIONAL-COVERAGE-0.9-PL.md)
- [Functional coverage EN](docs/FUNCTIONAL-COVERAGE-0.9-EN.md)
- [Home Assistant PL](docs/HOME-ASSISTANT-PL.md)
- [Home Assistant EN](docs/HOME-ASSISTANT-EN.md)
- [Flashing](docs/FLASHING.md)
