#pragma once

#include <Arduino.h>
#include <Preferences.h>

// Device-wide settings only. Diagnostic objects and rules belong to LogicModelStore.
struct DeviceConfig {
  uint32_t schemaVersion = 1;
  bool useDhcp = true;
  char deviceIp[16] = "192.168.1.50";
  char subnetMask[16] = "255.255.255.0";
  char deviceGateway[16] = "192.168.1.1";
  char primaryDns[16] = "1.1.1.1";
  uint16_t diagnosticIntervalSeconds = 15;
  uint16_t startupDelaySeconds = 120;
  bool automationEnabled = true;
  char webPassword[33] = "admin";
};

class DeviceConfigStore {
 public:
  void begin();
  DeviceConfig snapshot() const;
  uint32_t sequence() const;
  bool update(const DeviceConfig& candidate, char* error, size_t errorSize);
  static bool validate(const DeviceConfig& candidate, char* error, size_t errorSize);
  static DeviceConfig defaults();

 private:
  Preferences preferences_;
  DeviceConfig config_{};
  uint32_t sequence_ = 0;
  bool activeSlotA_ = false;
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  bool save(const DeviceConfig& value);
};
