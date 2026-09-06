#include "logic_engine.h"

#include <ESP32Ping.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"
#include "device_config.h"
#include "power_module.h"

namespace {
SemaphoreHandle_t diagnosticMutex() {
  static SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
  return mutex;
}

bool resolveAddress(const char* value, IPAddress& address) {
  return address.fromString(value) || WiFi.hostByName(value, address) == 1;
}

bool runHttpTest(const DiagnosticTarget& target, const IPAddress& address,
                 uint16_t& responseCode, float& elapsedMs) {
  WiFiClient plain;
  WiFiClientSecure secure;
  Client* client = target.test == DiagnosticTest::Https
                       ? static_cast<Client*>(&secure)
                       : static_cast<Client*>(&plain);
  if (target.test == DiagnosticTest::Https) secure.setInsecure();
  client->setTimeout(target.timeoutMs);
  const uint16_t port = target.port ? target.port
                                    : (target.test == DiagnosticTest::Https ? 443 : 80);
  const uint32_t started = millis();
  const bool connected = target.test == DiagnosticTest::Https
      ? secure.connect(address, port, target.timeoutMs)
      : plain.connect(address, port, target.timeoutMs);
  if (!connected) {
    elapsedMs = millis() - started;
    return false;
  }
  client->print("HEAD ");
  client->print(target.path);
  client->print(" HTTP/1.1\r\nHost: ");
  client->print(target.address);
  client->print("\r\nConnection: close\r\nUser-Agent: MarkLabs-NetGuard\r\n\r\n");
  String line = client->readStringUntil('\n');
  elapsedMs = millis() - started;
  client->stop();
  line.trim();
  const int separator = line.indexOf(' ');
  if (!line.startsWith("HTTP/") || separator < 0) return false;
  responseCode = line.substring(separator + 1).toInt();
  return responseCode == target.expectedHttpCode;
}
}  // namespace

void LogicEngine::begin() {
  startedAt_ = millis();
  recoveryPreferences_.begin("ng-recovery", false);
  if (recoveryPreferences_.getBytesLength("counts") == sizeof(executionsInWindow_)) {
    recoveryPreferences_.getBytes("counts", executionsInWindow_, sizeof(executionsInWindow_));
    for (uint8_t i = 0; i < kPoweredDeviceCount; ++i) {
      if (executionsInWindow_[i]) {
        windowStarted_[i] = startedAt_;
        lastExecution_[i] = startedAt_;
      }
    }
  }
  const BaseType_t created=xTaskCreatePinnedToCore(taskEntry,"logic-diag",10240,this,1,&diagnosticTask_,0);
  if(created!=pdPASS){
    diagnosticTask_=nullptr;
    Serial.println("FATAL LOGIC TASK CREATE FAILED");
  }
}

void LogicEngine::saveRecoveryCounters() {
  recoveryPreferences_.putBytes("counts", executionsInWindow_, sizeof(executionsInWindow_));
}

void LogicEngine::setNetworkReady(bool ready) {
  portENTER_CRITICAL(&mux_);
  networkReady_ = ready;
  portEXIT_CRITICAL(&mux_);
}

void LogicEngine::requestRun() { runRequested_ = true; }
void LogicEngine::setOperationsInhibited(bool inhibited){
  operationsInhibited_=inhibited;
  portENTER_CRITICAL(&mux_);
  state_.operationsInhibited=inhibited;
  portEXIT_CRITICAL(&mux_);
}

bool LogicEngine::requestManualPulse(uint8_t deviceIndex, uint32_t now, char* error,
                                     size_t errorSize) {
  const auto fail=[error,errorSize](const char* message){if(errorSize)strlcpy(error,message,errorSize);return false;};
  if(deviceIndex>=kPoweredDeviceCount)return fail("INVALID_DEVICE");
  if(operationsInhibited_)return fail("OPERATIONS_INHIBITED");
  const LogicModel model=model_.snapshot();
  const PoweredDevice& device=model.devices[deviceIndex];
  if(!device.enabled)return fail("DEVICE_DISABLED");
  if(!power_.online()||power_.consecutiveHeartbeats()<3)return fail("POWER_MODULE_OFFLINE");
  if(pulsePending_||power_.commandPending())return fail("POWER_BUSY");
  const DeviceConfig systemConfig=deviceConfig_.snapshot();
  if(now-startedAt_<static_cast<uint32_t>(systemConfig.startupDelaySeconds)*1000U)return fail("STARTUP_DELAY");
  if(stabilizationUntil_[deviceIndex]&&static_cast<int32_t>(stabilizationUntil_[deviceIndex]-now)>0)return fail("STABILIZING");
  if(lastConfirmedPulseAt_&&now-lastConfirmedPulseAt_<config::kGlobalPulseSpacingMs)return fail("GLOBAL_SPACING");
  if(lastExecution_[deviceIndex]&&now-lastExecution_[deviceIndex]<static_cast<uint32_t>(device.cooldownSeconds)*1000U)return fail("RETRY_DELAY");
  if(device.maximumRecoveriesPerHour&&executionsInWindow_[deviceIndex]>=device.maximumRecoveriesPerHour)return fail("HOURLY_LIMIT");
  acceptedBaseline_=power_.acceptedPulses();
  failureBaseline_=power_.commandFailures();
  if(!power_.requestPulse(device.powerChannel+1,device.pulseSeconds,now))return fail("POWER_BUSY");
  pulsePending_=true;
  pendingDevice_=deviceIndex;
  char message[64];
  snprintf(message,sizeof(message),"MANUAL RESET DEVICE %u CH%u",deviceIndex+1,device.powerChannel+1);
  portENTER_CRITICAL(&mux_);
  ++state_.requested;
  state_.pulsePending=true;
  state_.pendingDevice=deviceIndex;
  portEXIT_CRITICAL(&mux_);
  recordDecision(now,message);
  if(errorSize)strlcpy(error,"OK",errorSize);
  return true;
}

LogicEngineSnapshot LogicEngine::snapshot() const {
  portENTER_CRITICAL(&mux_);
  const LogicEngineSnapshot copy = state_;
  portEXIT_CRITICAL(&mux_);
  return copy;
}

void LogicEngine::recordDecision(uint32_t now, const char* message) {
  portENTER_CRITICAL(&mux_);
  strlcpy(state_.lastDecision, message, sizeof(state_.lastDecision));
  state_.lastDecisionAtMs = now;
  if (state_.eventCount < 16) {
    state_.events[state_.eventCount].timestampMs = now;
    strlcpy(state_.events[state_.eventCount].message, message,
            sizeof(state_.events[state_.eventCount].message));
    ++state_.eventCount;
  } else {
    memmove(&state_.events[0], &state_.events[1], sizeof(LogicEvent) * 15);
    state_.events[15].timestampMs = now;
    strlcpy(state_.events[15].message, message, sizeof(state_.events[15].message));
  }
  portEXIT_CRITICAL(&mux_);
}

void LogicEngine::taskEntry(void* argument) {
  static_cast<LogicEngine*>(argument)->taskLoop();
}

void LogicEngine::taskLoop() {
  for (;;) {
    bool networkReady;
    portENTER_CRITICAL(&mux_);
    networkReady = networkReady_;
    portEXIT_CRITICAL(&mux_);
    if (!networkReady) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    xSemaphoreTake(diagnosticMutex(), portMAX_DELAY);
    const LogicModel model = model_.snapshot();
    const uint32_t modelSequence = model_.sequence();
    LogicEngineSnapshot next = snapshot();
    next.modelSequence = modelSequence;
    next.running = true;
    portENTER_CRITICAL(&mux_);
    state_.running = true;
    portEXIT_CRITICAL(&mux_);

    for (uint8_t i = 0; i < kDiagnosticTargetCount; ++i) {
      const DiagnosticTarget& target = model.targets[i];
      LogicTargetResult& result = next.targets[i];
      result.enabled = target.enabled;
      result.ok = false;
      result.responseMs = 0;
      result.responseCode = 0;
      if (!target.enabled) continue;

      const uint32_t started = millis();
      IPAddress address;
      bool ok = false;
      if (target.test == DiagnosticTest::Dns) {
        ok = WiFi.hostByName(target.address, address) == 1;
        result.responseMs = millis() - started;
      } else if (resolveAddress(target.address, address)) {
        if (target.test == DiagnosticTest::Ping) {
          ok = Ping.ping(address, 2);
          result.responseMs = ok ? Ping.averageTime() : millis() - started;
          if (ok && result.responseMs > target.timeoutMs) ok = false;
        } else if (target.test == DiagnosticTest::Tcp) {
          WiFiClient client;
          ok = client.connect(address, target.port ? target.port : 80, target.timeoutMs);
          result.responseMs = millis() - started;
          client.stop();
        } else {
          ok = runHttpTest(target, address, result.responseCode, result.responseMs);
        }
      } else {
        result.responseMs = millis() - started;
      }
      result.ok = ok;
      if (result.checks < UINT32_MAX) ++result.checks;
      if (ok && result.successes < UINT32_MAX) ++result.successes;
    }
    xSemaphoreGive(diagnosticMutex());

    // Never publish a mixed snapshot if configuration changed during a cycle.
    if (modelSequence != model_.sequence()) {
      portENTER_CRITICAL(&mux_);
      state_.running = false;
      portEXIT_CRITICAL(&mux_);
      continue;
    }
    next.valid = true;
    next.running = false;
    ++next.completedRuns;
    portENTER_CRITICAL(&mux_);
    next.requested = state_.requested;
    next.executed = state_.executed;
    next.failed = state_.failed;
    next.operationsInhibited = state_.operationsInhibited;
    strlcpy(next.lastDecision, state_.lastDecision, sizeof(next.lastDecision));
    next.lastDecisionAtMs = state_.lastDecisionAtMs;
    next.pulsePending = state_.pulsePending;
    next.pendingDevice = state_.pendingDevice;
    memcpy(next.devices, state_.devices, sizeof(next.devices));
    memcpy(next.rules, state_.rules, sizeof(next.rules));
    memcpy(next.events, state_.events, sizeof(next.events));
    next.eventCount = state_.eventCount;
    state_ = next;
    portEXIT_CRITICAL(&mux_);

    const uint16_t interval = deviceConfig_.snapshot().diagnosticIntervalSeconds;
    for (uint16_t waited = 0; waited < interval; ++waited) {
      if (runRequested_) { runRequested_ = false; break; }
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

void LogicEngine::update(uint32_t now) {
  const auto setRuleRuntime = [this](uint8_t index, const LogicRuleRuntime& runtime) {
    portENTER_CRITICAL(&mux_);
    state_.rules[index] = runtime;
    portEXIT_CRITICAL(&mux_);
  };

  bool networkReady;
  portENTER_CRITICAL(&mux_);
  networkReady = networkReady_;
  state_.pulsePending = pulsePending_;
  state_.pendingDevice = pendingDevice_;
  portEXIT_CRITICAL(&mux_);
  if (!networkReady) {
    memset(matchingCycles_, 0, sizeof(matchingCycles_));
    return;
  }

  if (pulsePending_) {
    if (power_.acceptedPulses() != acceptedBaseline_) {
      pulsePending_ = false;
      lastExecution_[pendingDevice_] = now;
      lastConfirmedPulseAt_ = now;
      const LogicModel currentModel = model_.snapshot();
      stabilizationUntil_[pendingDevice_] = now +
          static_cast<uint32_t>(currentModel.devices[pendingDevice_].stabilizationSeconds) * 1000U;
      if (!windowStarted_[pendingDevice_] || now - windowStarted_[pendingDevice_] >= 3600000U) {
        windowStarted_[pendingDevice_] = now;
        executionsInWindow_[pendingDevice_] = 0;
      }
      if (executionsInWindow_[pendingDevice_] < UINT8_MAX)
        ++executionsInWindow_[pendingDevice_];
      saveRecoveryCounters();
      char message[64];
      snprintf(message, sizeof(message), "RESET CONFIRMED DEVICE %u", pendingDevice_ + 1);
      portENTER_CRITICAL(&mux_);
      ++state_.executed;
      state_.pulsePending = false;
      portEXIT_CRITICAL(&mux_);
      recordDecision(now, message);
    } else if (power_.commandFailures() != failureBaseline_) {
      pulsePending_ = false;
      char message[64];
      snprintf(message, sizeof(message), "RESET FAILED DEVICE %u", pendingDevice_ + 1);
      portENTER_CRITICAL(&mux_);
      ++state_.failed;
      state_.pulsePending = false;
      portEXIT_CRITICAL(&mux_);
      recordDecision(now, message);
    } else {
      return;
    }
  }

  const LogicEngineSnapshot measured = snapshot();
  const uint32_t currentSequence = model_.sequence();
  if (!measured.valid || measured.running || measured.completedRuns == handledRun_ ||
      measured.modelSequence != currentSequence)
    return;
  handledRun_ = measured.completedRuns;
  const LogicModel model = model_.snapshot();
  const DeviceConfig systemConfig = deviceConfig_.snapshot();
  if (evaluatedModelSequence_ != currentSequence) {
    memset(matchingCycles_, 0, sizeof(matchingCycles_));
    evaluatedModelSequence_ = currentSequence;
  }

  LogicDeviceRuntime deviceRuntime[kPoweredDeviceCount]{};
  bool recoveryCountersChanged=false;
  for (uint8_t i = 0; i < kPoweredDeviceCount; ++i) {
    if (windowStarted_[i] && now - windowStarted_[i] >= 3600000U) {
      windowStarted_[i] = now;
      executionsInWindow_[i] = 0;
      recoveryCountersChanged=true;
    }
    if (stabilizationUntil_[i] && static_cast<int32_t>(stabilizationUntil_[i] - now) > 0)
      deviceRuntime[i].stabilizationRemainingSeconds = (stabilizationUntil_[i] - now + 999U) / 1000U;
    if (lastExecution_[i]) {
      const uint32_t cooldownMs = static_cast<uint32_t>(model.devices[i].cooldownSeconds) * 1000U;
      if (now - lastExecution_[i] < cooldownMs)
        deviceRuntime[i].cooldownRemainingSeconds = (cooldownMs - (now - lastExecution_[i]) + 999U) / 1000U;
    }
    deviceRuntime[i].recoveriesInCurrentHour = executionsInWindow_[i];
  }
  if(recoveryCountersChanged)saveRecoveryCounters();
  portENTER_CRITICAL(&mux_);
  memcpy(state_.devices, deviceRuntime, sizeof(deviceRuntime));
  portEXIT_CRITICAL(&mux_);

  for (uint8_t i = 0; i < kAutomationRuleCount; ++i) {
    const AutomationRule& rule = model.rules[i];
    LogicRuleRuntime runtime{};
    runtime.enabled = rule.enabled;
    runtime.requiredCycles = rule.consecutiveCycles;
    if (!rule.enabled) {
      matchingCycles_[i] = 0;
      setRuleRuntime(i, runtime);
      continue;
    }
    if (!systemConfig.automationEnabled) {
      matchingCycles_[i] = 0;
      runtime.state = LogicRuleState::Paused;
      setRuleRuntime(i, runtime);
      continue;
    }
    if (operationsInhibited_) {
      matchingCycles_[i] = 0;
      runtime.state = LogicRuleState::OperationsInhibited;
      setRuleRuntime(i, runtime);
      continue;
    }

    bool matched = rule.conditionOperator == ConditionOperator::All;
    for (uint8_t c = 0; c < rule.conditionCount; ++c) {
      const RuleCondition& condition = rule.conditions[c];
      if (condition.targetIndex >= kDiagnosticTargetCount) {
        matched = false;
        break;
      }
      const LogicTargetResult& target = measured.targets[condition.targetIndex];
      const bool value = target.enabled &&
          (target.ok == (condition.expectedState == ExpectedTargetState::Up));
      matched = rule.conditionOperator == ConditionOperator::All ? matched && value
                                                                  : matched || value;
    }
    runtime.matched = matched;
    if (!matched) {
      matchingCycles_[i] = 0;
      runtime.state = LogicRuleState::Watching;
      setRuleRuntime(i, runtime);
      continue;
    }
    if (matchingCycles_[i] < UINT8_MAX) ++matchingCycles_[i];
    runtime.matchingCycles = matchingCycles_[i];
    if (matchingCycles_[i] < rule.consecutiveCycles) {
      runtime.state = LogicRuleState::Counting;
      setRuleRuntime(i, runtime);
      continue;
    }

    const uint8_t deviceIndex = rule.poweredDeviceIndex;
    if (deviceIndex >= kPoweredDeviceCount) {
      runtime.state = LogicRuleState::Watching;
      setRuleRuntime(i, runtime);
      continue;
    }
    const PoweredDevice& device = model.devices[deviceIndex];
    if (!device.enabled || now - startedAt_ < static_cast<uint32_t>(systemConfig.startupDelaySeconds) * 1000U) {
      runtime.state = LogicRuleState::StartupDelay;
    } else if (!power_.online() || power_.consecutiveHeartbeats() < 3) {
      runtime.state = LogicRuleState::PowerUnavailable;
    } else if (deviceRuntime[deviceIndex].stabilizationRemainingSeconds) {
      runtime.state = LogicRuleState::Stabilizing;
    } else if (lastConfirmedPulseAt_ && now - lastConfirmedPulseAt_ < config::kGlobalPulseSpacingMs) {
      runtime.state = LogicRuleState::GlobalSpacing;
    } else if (deviceRuntime[deviceIndex].cooldownRemainingSeconds) {
      runtime.state = LogicRuleState::Cooldown;
    } else if (device.maximumRecoveriesPerHour &&
               executionsInWindow_[deviceIndex] >= device.maximumRecoveriesPerHour) {
      runtime.state = LogicRuleState::HourlyLimit;
    } else {
      acceptedBaseline_ = power_.acceptedPulses();
      failureBaseline_ = power_.commandFailures();
      if (!power_.requestPulse(device.powerChannel + 1, device.pulseSeconds, now)) {
        runtime.state = LogicRuleState::PowerBusy;
      } else {
        pulsePending_ = true;
        pendingDevice_ = deviceIndex;
        matchingCycles_[i] = 0;
        runtime.matchingCycles = 0;
        runtime.state = LogicRuleState::PulseRequested;
        char message[64];
        snprintf(message, sizeof(message), "RULE %u RESET CH%u", i + 1,
                 device.powerChannel + 1);
        portENTER_CRITICAL(&mux_);
        ++state_.requested;
        state_.pulsePending = true;
        state_.pendingDevice = deviceIndex;
        portEXIT_CRITICAL(&mux_);
        recordDecision(now, message);
        setRuleRuntime(i, runtime);
        break;
      }
    }
    setRuleRuntime(i, runtime);
  }
}
