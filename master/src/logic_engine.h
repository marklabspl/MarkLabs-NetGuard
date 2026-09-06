#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "logic_model.h"

struct LogicTargetResult {
  bool enabled=false;
  bool ok=false;
  float responseMs=0;
  uint16_t responseCode=0;
  uint32_t checks=0;
  uint32_t successes=0;
};

enum class LogicRuleState : uint8_t {
  Disabled,
  Watching,
  Counting,
  StartupDelay,
  PowerUnavailable,
  Stabilizing,
  GlobalSpacing,
  Cooldown,
  HourlyLimit,
  PowerBusy,
  PulseRequested,
  Paused,
  OperationsInhibited
};

struct LogicDeviceRuntime {
  uint32_t stabilizationRemainingSeconds=0;
  uint32_t cooldownRemainingSeconds=0;
  uint8_t recoveriesInCurrentHour=0;
};

struct LogicRuleRuntime {
  bool enabled=false;
  bool matched=false;
  uint8_t matchingCycles=0;
  uint8_t requiredCycles=0;
  LogicRuleState state=LogicRuleState::Disabled;
};

struct LogicEvent {
  uint32_t timestampMs=0;
  char message[64]{};
};

struct LogicEngineSnapshot {
  bool valid=false;
  bool running=false;
  bool operationsInhibited=false;
  uint32_t completedRuns=0;
  uint32_t modelSequence=0;
  LogicTargetResult targets[kDiagnosticTargetCount]{};
  LogicDeviceRuntime devices[kPoweredDeviceCount]{};
  LogicRuleRuntime rules[kAutomationRuleCount]{};
  uint32_t requested=0;
  uint32_t executed=0;
  uint32_t failed=0;
  char lastDecision[64]{"NONE"};
  uint32_t lastDecisionAtMs=0;
  bool pulsePending=false;
  uint8_t pendingDevice=0;
  LogicEvent events[16]{};
  uint8_t eventCount=0;
};

class LogicEngine {
 public:
  LogicEngine(LogicModelStore& model,class DeviceConfigStore& deviceConfig,class PowerModule& power):model_(model),deviceConfig_(deviceConfig),power_(power){}
  void begin();
  void setNetworkReady(bool ready);
  void requestRun();
  bool requestManualPulse(uint8_t deviceIndex,uint32_t now,char* error,size_t errorSize);
  void setOperationsInhibited(bool inhibited);
  void update(uint32_t now);
  LogicEngineSnapshot snapshot() const;
 private:
  static void taskEntry(void* argument);
  void taskLoop();
  void recordDecision(uint32_t now,const char* message);
  LogicModelStore& model_;
  DeviceConfigStore& deviceConfig_;
  PowerModule& power_;
  mutable portMUX_TYPE mux_=portMUX_INITIALIZER_UNLOCKED;
  LogicEngineSnapshot state_{};
  bool networkReady_=false;
  volatile bool runRequested_=false;
  volatile bool operationsInhibited_=false;
  TaskHandle_t diagnosticTask_=nullptr;
  uint32_t handledRun_=0,startedAt_=0;
  uint32_t evaluatedModelSequence_=0;
  uint8_t matchingCycles_[kAutomationRuleCount]{};
  uint32_t lastExecution_[kPoweredDeviceCount]{};
  uint32_t windowStarted_[kPoweredDeviceCount]{};
  uint8_t executionsInWindow_[kPoweredDeviceCount]{};
  uint32_t stabilizationUntil_[kPoweredDeviceCount]{};
  bool pulsePending_=false;
  uint8_t pendingDevice_=0;
  uint32_t acceptedBaseline_=0,failureBaseline_=0;
  uint32_t lastConfirmedPulseAt_=0;
  Preferences recoveryPreferences_;
  void saveRecoveryCounters();
};
