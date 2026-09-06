#pragma once

#include <Arduino.h>

class DeviceConfigStore;
class NetworkMonitor {
 public:
  explicit NetworkMonitor(DeviceConfigStore& config) : config_(config) {}
  void begin();
  void update(uint32_t now);
  bool linkUp() const { return linkUp_; }
  bool hasIp() const { return hasIp_; }
  const char* ipAddress() const { return ipAddress_; }
  const char* macAddress() const { return macAddress_; }
  uint32_t linkChanges() const { return linkChanges_; }
  uint16_t linkSpeedMbps() const { return linkSpeedMbps_; }
  bool fullDuplex() const { return fullDuplex_; }

 private:
  bool linkUp_ = false;
  bool hasIp_ = false;
  char ipAddress_[16] = "0.0.0.0";
  char macAddress_[18] = "00:00:00:00:00:00";
  uint32_t linkChanges_ = 0;
  uint16_t linkSpeedMbps_ = 0;
  bool fullDuplex_ = false;
  DeviceConfigStore& config_;
};
