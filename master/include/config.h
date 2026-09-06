#pragma once

#include <stdint.h>

namespace config {

constexpr char kProduct[] = "NETGUARD-MASTER";
constexpr char kVersion[] = "0.9.0-rc.6";
constexpr char kOperatingMode[] = "PRODUCTION";

constexpr int kEthPhyAddress = 1;
constexpr int kEthPowerPin = 16;
constexpr int kEthMdcPin = 23;
constexpr int kEthMdioPin = 18;

constexpr int kPowerUartRxPin = 32;
constexpr int kPowerUartTxPin = 33;
constexpr uint32_t kPowerUartBaud = 9600;
constexpr uint32_t kHeartbeatIntervalMs = 5000;
constexpr uint32_t kHeartbeatTimeoutMs = 1200;
constexpr uint32_t kResetCompletionMarginMs = 5000;

constexpr uint16_t kHttpPort = 80;
constexpr char kWebAdminUser[] = "admin";
constexpr char kWebAdminPassword[] = "admin";
constexpr uint32_t kGlobalPulseSpacingMs = 15000;

}  // namespace config
