# Verification report 0.3.1

- PlatformIO `sonoff_4chr3` build: **PASS**.
- Target: ESP8266/ESP8285, 80 MHz, 1 MB flash, 40 MHz, DOUT.
- RAM: 28,708 / 81,920 bytes (35.1%).
- Program: 270,983 / 958,448 bytes (28.3%).
- BIN: 275,136 bytes.
- Host behavioral protocol-v2 suite: **PASS**.
- Target Xtensa compilation of the production parser: **PASS**.

The native C++ runner could not start because system GCC/G++ is not installed on the
build host. This is an environment limitation rather than a test failure in target
code. Physical relay, brownout and long-duration tests remain to be performed on the
updated unit.
