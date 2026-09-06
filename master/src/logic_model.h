#pragma once

#include <Arduino.h>
#include <Preferences.h>

constexpr uint8_t kPoweredDeviceCount = 4;
constexpr uint8_t kDiagnosticTargetCount = 12;
constexpr uint8_t kAutomationRuleCount = 8;
constexpr uint8_t kRuleConditionCount = 4;

enum class ObjectIcon : uint8_t {
  Generic, Router, Switch, AccessPoint, Modem, Server, Nas, Camera, Internet,
  Dns, Gateway, Website
};

enum class DiagnosticTest : uint8_t { Ping, Tcp, Http, Https, Dns };
enum class ExpectedTargetState : uint8_t { Up, Down };
enum class ConditionOperator : uint8_t { All, Any };

struct PoweredDevice {
  bool enabled = false;
  char name[32]{};
  ObjectIcon icon = ObjectIcon::Generic;
  uint8_t powerChannel = 0;
  uint16_t pulseSeconds = 10;
  uint16_t stabilizationSeconds = 60;
  uint16_t cooldownSeconds = 300;
  uint8_t maximumRecoveriesPerHour = 3;
};

struct DiagnosticTarget {
  bool enabled = false;
  char name[32]{};
  ObjectIcon icon = ObjectIcon::Generic;
  char address[64]{};
  DiagnosticTest test = DiagnosticTest::Ping;
  uint16_t port = 0;
  char path[48]{"/"};
  uint16_t expectedHttpCode = 200;
  uint16_t timeoutMs = 1000;
};

struct RuleCondition {
  uint8_t targetIndex = 0;
  ExpectedTargetState expectedState = ExpectedTargetState::Up;
};

struct AutomationRule {
  bool enabled = false;
  char name[40]{};
  ConditionOperator conditionOperator = ConditionOperator::All;
  uint8_t conditionCount = 0;
  RuleCondition conditions[kRuleConditionCount]{};
  uint8_t consecutiveCycles = 3;
  uint8_t poweredDeviceIndex = 0;
};

struct LogicModel {
  uint32_t schemaVersion = 1;
  PoweredDevice devices[kPoweredDeviceCount]{};
  DiagnosticTarget targets[kDiagnosticTargetCount]{};
  AutomationRule rules[kAutomationRuleCount]{};
};

class LogicModelValidator {
 public:
  static LogicModel defaults();
  static bool validate(const LogicModel& model, char* error, size_t errorSize);
};

class LogicModelStore {
 public:
  void begin();
  LogicModel snapshot() const;
  bool update(const LogicModel& model, char* error, size_t errorSize);
  uint32_t sequence() const;

 private:
  Preferences preferences_;
  LogicModel model_{};
  uint32_t sequence_ = 0;
  bool activeSlotA_ = false;
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  bool save(const LogicModel& model);
};
