# Production commissioning checklist

Version: `0.9.0-rc.6`, mode: `PRODUCTION`.

1. Change the default panel password.
2. Configure networking and verify panel access.
3. Map devices uniquely to CH1–CH4.
4. Verify all diagnostic targets.
5. Set RESET, stabilization, retry delay and hourly limit.
6. Verify every rule and its cycle count.
7. Confirm UART and one RESET on every output.
8. Configure MQTT credentials and verify Discovery, LWT and every HA command.
9. Enable automation after checking mappings and the event log.
