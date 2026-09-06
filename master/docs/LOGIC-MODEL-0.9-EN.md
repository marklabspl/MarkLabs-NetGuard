# NetGuard 0.9 logic model

The system manages four physical devices. Every enabled device has a unique CH1–CH4
output, name, icon, RESET duration, stabilization time, retry delay and optional
hourly operation limit.

Up to 12 independent targets may use PING, TCP, HTTP, HTTPS or DNS. A target is not
a powered outlet and may be reused by multiple rules.

Up to eight rules combine one to four conditions using ALL or ANY. After the required
matching cycles, a rule may request a device RESET. Home Assistant receives the same
results and can request manual RESET through the guarded `LogicEngine` path; it does
not control a CH output directly.
