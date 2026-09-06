#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct HomeAssistantConfig {
  uint32_t schemaVersion = 2;
  bool enabled = false;
  bool tls = false;
  char broker[64] = "";
  uint16_t port = 1883;
  char username[33] = "";
  char password[65] = "";
  char deviceName[33] = "MarkLabs NetGuard";
  char discoveryPrefix[33] = "homeassistant";
  char topicPrefix[65] = "marklabs/netguard";
  char language[3] = "pl";
};

class HomeAssistantConfigStore {
 public:
  void begin();
  HomeAssistantConfig snapshot() const;
  uint32_t sequence() const;
  bool update(const HomeAssistantConfig& candidate, char* error, size_t errorSize);
  static bool validate(const HomeAssistantConfig& value, char* error, size_t errorSize);

 private:
  Preferences preferences_;
  HomeAssistantConfig config_{};
  uint32_t sequence_ = 0;
  bool activeSlotA_ = false;
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  bool save(const HomeAssistantConfig& value);
};
