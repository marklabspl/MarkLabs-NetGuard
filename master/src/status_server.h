#pragma once

#include <WebServer.h>

class NetworkMonitor;
class PowerModule;
class DeviceConfigStore;
class LogicModelStore;
class LogicEngine;
class HomeAssistantConfigStore;
class HomeAssistantMqtt;

class StatusServer {
 public:
  StatusServer(NetworkMonitor& network,PowerModule& power,DeviceConfigStore& config,
               LogicModelStore& logic,LogicEngine& logicEngine,
               HomeAssistantConfigStore& homeAssistantConfig,HomeAssistantMqtt& homeAssistant);
  void begin();
  void update();

 private:
  WebServer server_;
  NetworkMonitor& network_;
  PowerModule& power_;
  DeviceConfigStore& config_;
  LogicModelStore& logic_;
  LogicEngine& logicEngine_;
  HomeAssistantConfigStore& homeAssistantConfig_;
  HomeAssistantMqtt& homeAssistant_;
  bool otaAuthenticated_=false;
  bool otaHeaderValidated_=false;
  char otaError_[48]{};
  struct LoginAttempt{IPAddress address;uint8_t failures=0;uint32_t blockedUntil=0;uint32_t lastSeen=0;};
  LoginAttempt loginAttempts_[4]{};
  struct SystemEvent{uint32_t timestampMs;char code[24];bool state;};
  SystemEvent systemEvents_[24]{};
  uint8_t systemEventCount_=0;
  bool observedStateReady_=false;
  bool previousLink_=false;
  bool previousPower_=false;

  bool requireAuthentication();
  bool requireSameOrigin();
  bool requireChangedPassword();
  void sendSecurityHeaders();
  void handleRoot();
  void handleStatus();
  void handleConfigPage();
  void handleConfigGet();
  void handleConfigPost();
  void handleBackupPage();
  void handleHomeAssistantPage();
  void handleHomeAssistantGet();
  void handleHomeAssistantPost();
  void handleHomeAssistantStatus();
  void handleLogicPage();
  void handleLogicGet();
  void handleLogicPost();
  void handleLogicStatus();
  void handleManualTest();
  void handleAutomationPost();
  void handleManualPulse();
  void handleSystemEvents();
  void handleRestart();
  void handleUpdatePage();
  void handleUpdateUpload();
  void handleUpdateFinished();
  void addSystemEvent(const char* code,bool state);
  void trackSystemEvents();
};
