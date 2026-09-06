#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

class DeviceConfigStore;
class HomeAssistantConfigStore;
class LogicEngine;
class LogicModelStore;
class NetworkMonitor;
class PowerModule;

struct HomeAssistantMqttStatus {
  bool enabled = false;
  bool connected = false;
  int8_t state = 0;
  uint32_t reconnects = 0;
  uint32_t published = 0;
  uint32_t received = 0;
};

class HomeAssistantMqtt {
 public:
  HomeAssistantMqtt(HomeAssistantConfigStore& config, DeviceConfigStore& deviceConfig,
                    LogicModelStore& model, LogicEngine& engine,
                    NetworkMonitor& network, PowerModule& power);
  void begin();
  void update(uint32_t now);
  HomeAssistantMqttStatus status() const;
  void beforeConfigurationChange();
  void configurationChanged();

 private:
  HomeAssistantConfigStore& config_;
  DeviceConfigStore& deviceConfig_;
  LogicModelStore& model_;
  LogicEngine& engine_;
  NetworkMonitor& network_;
  PowerModule& power_;
  WiFiClient plainClient_;
  WiFiClientSecure secureClient_;
  PubSubClient mqtt_;
  HomeAssistantMqttStatus status_{};
  uint32_t nextConnectAt_ = 0;
  uint32_t nextPublishAt_ = 0;
  uint32_t configSequence_ = UINT32_MAX;
  uint32_t modelSequence_ = UINT32_MAX;
  bool discoveryPending_ = false;
  bool discoveryPublishFailed_ = false;
  char nodeId_[32] = "netguard";
  char activeAvailabilityTopic_[128]{};
  char lastCommandResult_[64] = "NONE";

  static HomeAssistantMqtt* instance_;
  static void callback(char* topic, uint8_t* payload, unsigned int length);
  void handleMessage(const char* topic, const uint8_t* payload, unsigned int length);
  void connect(uint32_t now);
  void subscribeCommands();
  void publishDiscovery();
  void removeDiscovery();
  void publishState(uint32_t now);
  bool publish(const String& topic, const String& payload, bool retained);
  void publishEntity(const char* component, const String& objectId, const String& payload);
  String baseTopic() const;
  String availabilityTopic() const;
  String stateTopic() const;
  String deviceJson() const;
};
