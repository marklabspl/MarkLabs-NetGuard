#include "home_assistant_mqtt.h"

#include <ETH.h>

#include "config.h"
#include "device_config.h"
#include "home_assistant_config.h"
#include "logic_engine.h"
#include "logic_model.h"
#include "network_monitor.h"
#include "power_module.h"

HomeAssistantMqtt* HomeAssistantMqtt::instance_ = nullptr;

namespace {
String jsonEscape(const char* value) {
  String output;
  output.reserve(strlen(value) + 8);
  for (const char* p = value; *p; ++p) {
    const char c = *p;
    if (c == '\\' || c == '"') { output += '\\'; output += c; }
    else if (c == '\n') output += "\\n";
    else if (c == '\r') output += "\\r";
    else if (static_cast<uint8_t>(c) >= 0x20) output += c;
  }
  return output;
}
bool due(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}
const char* ruleStateName(LogicRuleState state, bool polish) {
  static const char* english[] = {"DISABLED", "WATCHING", "COUNTING", "STARTUP DELAY",
    "POWER OFFLINE", "STABILIZING", "GLOBAL SPACING", "RETRY DELAY", "HOURLY LIMIT",
    "POWER BUSY", "RESET SENT", "PAUSED", "OPERATIONS INHIBITED"};
  static const char* polishNames[] = {"WYŁĄCZONA", "CZUWA", "ODLICZA", "OCHRONA STARTOWA",
    "BRAK MODUŁU", "STABILIZACJA", "ODSTĘP GLOBALNY", "PONOWNA PRÓBA", "LIMIT GODZINOWY",
    "MODUŁ ZAJĘTY", "RESET WYSŁANY", "WSTRZYMANA", "OPERACJE ZABLOKOWANE"};
  const uint8_t index = static_cast<uint8_t>(state);
  return index < sizeof(english) / sizeof(english[0]) ?
    (polish ? polishNames[index] : english[index]) : (polish ? "NIEZNANY" : "UNKNOWN");
}
}  // namespace

HomeAssistantMqtt::HomeAssistantMqtt(HomeAssistantConfigStore& configStore,
                                     DeviceConfigStore& deviceConfig,
                                     LogicModelStore& model, LogicEngine& engine,
                                     NetworkMonitor& network, PowerModule& power)
    : config_(configStore), deviceConfig_(deviceConfig), model_(model), engine_(engine),
      network_(network), power_(power), mqtt_(plainClient_) {}

void HomeAssistantMqtt::begin() {
  instance_ = this;
  mqtt_.setCallback(callback);
  mqtt_.setBufferSize(2048);
  mqtt_.setKeepAlive(30);
  mqtt_.setSocketTimeout(3);
}

void HomeAssistantMqtt::callback(char* topic, uint8_t* payload, unsigned int length) {
  if (instance_) instance_->handleMessage(topic, payload, length);
}

String HomeAssistantMqtt::baseTopic() const {
  return String(config_.snapshot().topicPrefix) + "/" + nodeId_;
}
String HomeAssistantMqtt::availabilityTopic() const { return baseTopic() + "/availability"; }
String HomeAssistantMqtt::stateTopic() const { return baseTopic() + "/state"; }

bool HomeAssistantMqtt::publish(const String& topic, const String& payload, bool retained) {
  const bool ok = mqtt_.publish(topic.c_str(), payload.c_str(), retained);
  if (ok && status_.published < UINT32_MAX) ++status_.published;
  return ok;
}

String HomeAssistantMqtt::deviceJson() const {
  const HomeAssistantConfig settings = config_.snapshot();
  return String("{\"identifiers\":[\"") + nodeId_ + "\"],\"name\":\"" +
         jsonEscape(settings.deviceName) + "\",\"manufacturer\":\"MarkLabs.pl\",\"model\":\"NetGuard Master WT32-ETH01\",\"sw_version\":\"" +
         config::kVersion + "\",\"configuration_url\":\"http://" + network_.ipAddress() + "/\"}";
}

void HomeAssistantMqtt::publishEntity(const char* component, const String& objectId,
                                      const String& payload) {
  const HomeAssistantConfig settings = config_.snapshot();
  const String topic = String(settings.discoveryPrefix) + "/" + component + "/" +
                       nodeId_ + "/" + objectId + "/config";
  if (!publish(topic, payload, true)) discoveryPublishFailed_ = true;
}

void HomeAssistantMqtt::subscribeCommands() {
  const String base = baseTopic();
  const HomeAssistantConfig settings = config_.snapshot();
  mqtt_.subscribe((String(settings.discoveryPrefix) + "/status").c_str());
  mqtt_.subscribe((base + "/command/test").c_str());
  mqtt_.subscribe((base + "/command/automation").c_str());
  for (uint8_t i = 0; i < kPoweredDeviceCount; ++i)
    mqtt_.subscribe((base + "/command/reset/" + i).c_str());
}

void HomeAssistantMqtt::connect(uint32_t now) {
  const HomeAssistantConfig settings = config_.snapshot();
  status_.enabled = settings.enabled;
  if (!settings.enabled || !network_.hasIp() || !due(now, nextConnectAt_)) return;
  nextConnectAt_ = now + 5000U;
  mqtt_.setClient(settings.tls ? static_cast<Client&>(secureClient_) : static_cast<Client&>(plainClient_));
  if (settings.tls) secureClient_.setInsecure();
  mqtt_.setServer(settings.broker, settings.port);

  String mac = network_.macAddress();
  mac.replace(":", "");
  mac.toLowerCase();
  snprintf(nodeId_, sizeof(nodeId_), "netguard_%s", mac.c_str());
  const String clientId = String("netguard-") + mac;
  const String willTopic = availabilityTopic();
  const bool connected = mqtt_.connect(clientId.c_str(),
      settings.username[0] ? settings.username : nullptr,
      settings.password[0] ? settings.password : nullptr,
      willTopic.c_str(), 0, true, "offline");
  status_.state = mqtt_.state();
  status_.connected = connected;
  if (!connected) return;
  if (status_.reconnects < UINT32_MAX) ++status_.reconnects;
  strlcpy(activeAvailabilityTopic_, willTopic.c_str(), sizeof(activeAvailabilityTopic_));
  publish(willTopic, "online", true);
  subscribeCommands();
  discoveryPending_ = true;
  nextPublishAt_ = 0;
}

void HomeAssistantMqtt::handleMessage(const char* topic, const uint8_t* payload,
                                      unsigned int length) {
  if (status_.received < UINT32_MAX) ++status_.received;
  String value;
  value.reserve(length);
  for (unsigned int i = 0; i < length; ++i) value += static_cast<char>(payload[i]);
  value.trim();
  const String receivedTopic(topic);
  const bool polish = !strcmp(config_.snapshot().language, "pl");
  if (receivedTopic == String(config_.snapshot().discoveryPrefix) + "/status" && value == "online") {
    discoveryPending_ = true;
    return;
  }
  const String base = baseTopic();
  if (receivedTopic == base + "/command/test" && value == "PRESS") {
    engine_.requestRun();
    strlcpy(lastCommandResult_, polish ? "TEST PRZYJĘTY" : "TEST ACCEPTED", sizeof(lastCommandResult_));
    return;
  }
  if (receivedTopic == base + "/command/automation" && (value == "ON" || value == "OFF")) {
    DeviceConfig settings = deviceConfig_.snapshot();
    settings.automationEnabled = value == "ON";
    char error[48];
    if (deviceConfig_.update(settings, error, sizeof(error)))
      strlcpy(lastCommandResult_, polish ? (value == "ON" ? "AUTOMATYKA WŁĄCZONA" : "AUTOMATYKA WYŁĄCZONA") : (value == "ON" ? "AUTOMATION ON" : "AUTOMATION OFF"), sizeof(lastCommandResult_));
    else
      snprintf(lastCommandResult_, sizeof(lastCommandResult_), "AUTOMATION_FAILED_%s", error);
    nextPublishAt_ = 0;
    return;
  }
  for (uint8_t i = 0; i < kPoweredDeviceCount; ++i) {
    if (receivedTopic == base + "/command/reset/" + i && value == "PRESS") {
      char error[48];
      if (engine_.requestManualPulse(i, millis(), error, sizeof(error)))
        snprintf(lastCommandResult_, sizeof(lastCommandResult_), polish ? "RESET %u PRZYJĘTY" : "RESET %u ACCEPTED", i + 1);
      else
        snprintf(lastCommandResult_, sizeof(lastCommandResult_), polish ? "RESET %u ODRZUCONY: %s" : "RESET %u REJECTED: %s", i + 1, error);
      nextPublishAt_ = 0;
      return;
    }
  }
}

void HomeAssistantMqtt::publishDiscovery() {
  discoveryPublishFailed_ = false;
  const HomeAssistantConfig settings = config_.snapshot();
  const bool polish = !strcmp(settings.language, "pl");
  const String state = stateTopic(), availability = availabilityTopic(), device = deviceJson();
  const String common = String(",\"state_topic\":\"") + state + "\",\"availability_topic\":\"" + availability + "\",\"device\":" + device;
  auto sensor = [&](const String& id, const String& name, const String& valueTemplate,
                    const String& extra = "") {
    publishEntity("sensor", id, String("{\"name\":\"") + name + "\",\"unique_id\":\"" + nodeId_ + "_" + id + "\",\"value_template\":\"" + valueTemplate + "\"" + extra + common + "}");
  };
  auto binary = [&](const String& id, const String& name, const String& valueTemplate) {
    publishEntity("binary_sensor", id, String("{\"name\":\"") + name + "\",\"unique_id\":\"" + nodeId_ + "_" + id + "\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"value_template\":\"" + valueTemplate + "\"" + common + "}");
  };

  binary("ethernet", "Ethernet", "{{ 'ON' if value_json.ethernet else 'OFF' }}");
  binary("power_module", "Power Module", "{{ 'ON' if value_json.power_module else 'OFF' }}");
  sensor("ip", polish ? "Adres IP" : "IP address", "{{ value_json.ip }}", ",\"icon\":\"mdi:ip-network\"");
  sensor("uptime", polish ? "Czas pracy" : "Uptime", "{{ value_json.uptime }}", ",\"device_class\":\"duration\",\"unit_of_measurement\":\"s\"");
  sensor("free_heap", polish ? "Wolna pamięć" : "Free memory", "{{ value_json.free_heap }}", ",\"device_class\":\"data_size\",\"unit_of_measurement\":\"B\"");
  sensor("diagnostic_runs", polish ? "Cykle diagnostyczne" : "Diagnostic runs", "{{ value_json.runs }}");
  sensor("reset_ok", polish ? "Potwierdzone operacje RESET" : "Confirmed RESET operations", "{{ value_json.reset_ok }}");
  sensor("reset_failed", polish ? "Nieudane operacje RESET" : "Failed RESET operations", "{{ value_json.reset_failed }}");
  sensor("command_result", polish ? "Ostatnia komenda Home Assistant" : "Last Home Assistant command", "{{ value_json.command_result }}");

  const String base = baseTopic();
  publishEntity("switch", "automation", String("{\"name\":\"") + (polish ? "Automatyka" : "Automation") + "\",\"unique_id\":\"" + nodeId_ + "_automation\",\"command_topic\":\"" + base + "/command/automation\",\"state_topic\":\"" + state + "\",\"value_template\":\"{{ 'ON' if value_json.automation else 'OFF' }}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"availability_topic\":\"" + availability + "\",\"device\":" + device + "}");
  publishEntity("button", "run_test", String("{\"name\":\"") + (polish ? "Uruchom test diagnostyczny" : "Run diagnostic test") + "\",\"unique_id\":\"" + nodeId_ + "_run_test\",\"command_topic\":\"" + base + "/command/test\",\"payload_press\":\"PRESS\",\"availability_topic\":\"" + availability + "\",\"device\":" + device + "}");

  const LogicModel model = model_.snapshot();
  for (uint8_t i = 0; i < kDiagnosticTargetCount; ++i) {
    const String id = String("target_") + i;
    if (!model.targets[i].enabled) {
      publishEntity("binary_sensor", id, "");
      publishEntity("sensor", id + "_response", "");
      continue;
    }
    const String name = jsonEscape(model.targets[i].name);
    binary(id, name, String("{{ 'ON' if value_json.t") + i + " else 'OFF' }}");
    sensor(id + "_response", name + (polish ? " — czas odpowiedzi" : " response time"), String("{{ value_json.tm") + i + " }}", ",\"device_class\":\"duration\",\"unit_of_measurement\":\"ms\"");
  }
  for (uint8_t i = 0; i < kPoweredDeviceCount; ++i) {
    const String id = String("reset_") + i;
    if (!model.devices[i].enabled) {
      publishEntity("button", id, "");
      publishEntity("sensor", String("device_") + i, "");
      continue;
    }
    publishEntity("button", id, String("{\"name\":\"RESET ") + jsonEscape(model.devices[i].name) + "\",\"unique_id\":\"" + nodeId_ + "_" + id + "\",\"command_topic\":\"" + base + "/command/reset/" + i + "\",\"payload_press\":\"PRESS\",\"availability_topic\":\"" + availability + "\",\"device\":" + device + "}");
    sensor(String("device_") + i, jsonEscape(model.devices[i].name) + (polish ? " — stan" : " state"), String("{{ value_json.d") + i + " }}");
  }
  for (uint8_t i = 0; i < kAutomationRuleCount; ++i) {
    const String id = String("rule_") + i;
    if (!model.rules[i].enabled) { publishEntity("sensor", id, ""); continue; }
    sensor(id, jsonEscape(model.rules[i].name) + (polish ? " — stan" : " state"), String("{{ value_json.r") + i + " }}");
  }
  discoveryPending_ = discoveryPublishFailed_;
  modelSequence_ = model_.sequence();
}

void HomeAssistantMqtt::removeDiscovery() {
  if (!mqtt_.connected()) return;
  publishEntity("binary_sensor", "ethernet", "");
  publishEntity("binary_sensor", "power_module", "");
  const char* sensors[] = {"ip", "uptime", "free_heap", "diagnostic_runs", "reset_ok", "reset_failed", "command_result"};
  for (const char* sensor : sensors) publishEntity("sensor", sensor, "");
  publishEntity("switch", "automation", "");
  publishEntity("button", "run_test", "");
  for (uint8_t i = 0; i < kDiagnosticTargetCount; ++i) {
    const String id = String("target_") + i;
    publishEntity("binary_sensor", id, "");
    publishEntity("sensor", id + "_response", "");
  }
  for (uint8_t i = 0; i < kPoweredDeviceCount; ++i) {
    publishEntity("button", String("reset_") + i, "");
    publishEntity("sensor", String("device_") + i, "");
  }
  for (uint8_t i = 0; i < kAutomationRuleCount; ++i)
    publishEntity("sensor", String("rule_") + i, "");
  if (activeAvailabilityTopic_[0]) mqtt_.publish(activeAvailabilityTopic_, "offline", true);
}

void HomeAssistantMqtt::publishState(uint32_t now) {
  const LogicEngineSnapshot logic = engine_.snapshot();
  const DeviceConfig deviceConfig = deviceConfig_.snapshot();
  const HomeAssistantConfig haConfig = config_.snapshot();
  const bool polish = !strcmp(haConfig.language, "pl");
  String json;
  json.reserve(900);
  json = String("{\"ethernet\":") + (network_.hasIp() ? "true" : "false") +
         ",\"power_module\":" + (power_.online() ? "true" : "false") +
         ",\"automation\":" + (deviceConfig.automationEnabled ? "true" : "false") +
         ",\"ip\":\"" + network_.ipAddress() + "\",\"uptime\":" + now / 1000U +
         ",\"free_heap\":" + ESP.getFreeHeap() + ",\"runs\":" + logic.completedRuns +
         ",\"reset_ok\":" + logic.executed + ",\"reset_failed\":" + logic.failed;
  const char* commandResult = polish && !strcmp(lastCommandResult_, "NONE") ? "BRAK" : lastCommandResult_;
  json += String(",\"command_result\":\"") + jsonEscape(commandResult) + "\"";
  for (uint8_t i = 0; i < kDiagnosticTargetCount; ++i) {
    json += String(",\"t") + i + "\":" + (logic.targets[i].ok ? "true" : "false") +
            ",\"tm" + i + "\":" + String(logic.targets[i].responseMs, 1);
  }
  const LogicModel model = model_.snapshot();
  for (uint8_t i = 0; i < kPoweredDeviceCount; ++i) {
    const char* deviceState = !model.devices[i].enabled ? (polish ? "WYŁĄCZONE" : "DISABLED") :
      !power_.online() ? (polish ? "BRAK MODUŁU" : "POWER OFFLINE") :
      logic.pulsePending && logic.pendingDevice == i ? (polish ? "RESET W TOKU" : "RESET PENDING") :
      logic.devices[i].stabilizationRemainingSeconds ? (polish ? "STABILIZACJA" : "STABILIZING") :
      logic.devices[i].cooldownRemainingSeconds ? (polish ? "PONOWNA PRÓBA" : "RETRY DELAY") :
      model.devices[i].maximumRecoveriesPerHour &&
        logic.devices[i].recoveriesInCurrentHour >= model.devices[i].maximumRecoveriesPerHour ?
          (polish ? "LIMIT GODZINOWY" : "HOURLY LIMIT") : (polish ? "GOTOWE" : "READY");
    json += String(",\"d") + i + "\":\"" + deviceState + "\"";
  }
  for (uint8_t i = 0; i < kAutomationRuleCount; ++i)
    json += String(",\"r") + i + "\":\"" + ruleStateName(logic.rules[i].state, polish) + "\"";
  json += "}";
  publish(stateTopic(), json, true);
}

void HomeAssistantMqtt::configurationChanged() {
  configSequence_ = UINT32_MAX;
  discoveryPending_ = true;
  nextConnectAt_ = 0;
}

void HomeAssistantMqtt::beforeConfigurationChange() {
  removeDiscovery();
  if (mqtt_.connected()) mqtt_.disconnect();
  status_.connected = false;
}

void HomeAssistantMqtt::update(uint32_t now) {
  const HomeAssistantConfig settings = config_.snapshot();
  status_.enabled = settings.enabled;
  if (!settings.enabled || !network_.hasIp()) {
    if (mqtt_.connected()) {
      if (activeAvailabilityTopic_[0]) mqtt_.publish(activeAvailabilityTopic_, "offline", true);
      mqtt_.disconnect();
    }
    status_.connected = false;
    return;
  }
  if (configSequence_ != config_.sequence()) {
    configSequence_ = config_.sequence();
    if (mqtt_.connected()) mqtt_.disconnect();
    nextConnectAt_ = 0;
  }
  if (!mqtt_.connected()) {
    status_.connected = false;
    connect(now);
    return;
  }
  status_.connected = true;
  mqtt_.loop();
  if (modelSequence_ != model_.sequence()) discoveryPending_ = true;
  if (discoveryPending_) publishDiscovery();
  if (due(now, nextPublishAt_)) {
    nextPublishAt_ = now + 5000U;
    publishState(now);
  }
}

HomeAssistantMqttStatus HomeAssistantMqtt::status() const { return status_; }
