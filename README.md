# MarkLabs.pl NetGuard

NetGuard is a two-controller network monitoring and power-recovery system:

- `master/` — WT32-ETH01/ESP32 Ethernet controller, diagnostics, automation,
  web panel and Home Assistant MQTT integration;
- `power-module/` — SONOFF 4CHR3/ESP8266 four-channel power controller.

Compatible active versions:

- NETGUARD-MASTER `0.9.0-rc.6`;
- NETGUARD-POWER `0.3.1`;
- UART protocol v3 at 9600 baud, 8N1.

Install Power Module first and Master second. Keep automation disabled during a
mixed-version upgrade. See the bilingual documentation inside each component.

This repository is proprietary. See [LICENSE](LICENSE) and
[SECURITY.md](SECURITY.md).
