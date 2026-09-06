#include "logic_model.h"

#include <ctype.h>
#include <stddef.h>

namespace {
constexpr uint32_t kLogicMagic = 0x4E474C47;  // NGLG
struct StoredLogicModel { uint32_t magic; uint32_t sequence; LogicModel model; uint32_t crc; };
uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffffU;
  while (length--) { crc ^= *data++; for (uint8_t bit=0; bit<8; ++bit) crc=(crc>>1)^(0xedb88320U&(0U-(crc&1U))); }
  return ~crc;
}
bool validStored(const StoredLogicModel& value) {
  return value.magic == kLogicMagic && value.crc == crc32(reinterpret_cast<const uint8_t*>(&value), offsetof(StoredLogicModel, crc));
}
void setError(char* output, size_t size, const char* message) {
  if (!size) return;
  strlcpy(output, message, size);
}

bool validAddress(const char* value) {
  const size_t length = strlen(value);
  if (!length || length > 63 || value[0] == '.' || value[length - 1] == '.') return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = value[i];
    if (!(isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '.')) return false;
  }
  return true;
}
bool validLabel(const char* value, size_t capacity) {
  const size_t length = strnlen(value, capacity);
  if (!length || length >= capacity) return false;
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (c < 0x20 || c == '<' || c == '>' || c == '&' || c == '"' || c == '\'')
      return false;
  }
  return true;
}
bool validHttpPath(const char* value) {
  const size_t length = strnlen(value, 48);
  if (!length || length >= 48 || value[0] != '/') return false;
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (c < 0x21 || c == 0x7f || c == '"' || c == '\\') return false;
  }
  return true;
}
}

LogicModel LogicModelValidator::defaults() {
  LogicModel model{};
  for (uint8_t i = 0; i < kPoweredDeviceCount; ++i) {
    snprintf(model.devices[i].name, sizeof(model.devices[i].name), "Powered device %u", i + 1);
    model.devices[i].powerChannel = i;
  }
  strlcpy(model.targets[0].name, "Internet A", sizeof(model.targets[0].name));
  strlcpy(model.targets[0].address, "1.1.1.1", sizeof(model.targets[0].address));
  model.targets[0].icon = ObjectIcon::Internet;
  model.targets[0].enabled = true;
  strlcpy(model.targets[1].name, "Internet B", sizeof(model.targets[1].name));
  strlcpy(model.targets[1].address, "8.8.8.8", sizeof(model.targets[1].address));
  model.targets[1].icon = ObjectIcon::Internet;
  model.targets[1].enabled = true;
  return model;
}

bool LogicModelValidator::validate(const LogicModel& model, char* error, size_t errorSize) {
  if (model.schemaVersion != 1) { setError(error, errorSize, "INVALID_LOGIC_SCHEMA"); return false; }
  bool channels[kPoweredDeviceCount]{};
  for (const PoweredDevice& device : model.devices) {
    if (!validLabel(device.name, sizeof(device.name)) || static_cast<uint8_t>(device.icon)>static_cast<uint8_t>(ObjectIcon::Website) || device.powerChannel >= kPoweredDeviceCount ||
        device.pulseSeconds < 1 || device.pulseSeconds > 300 ||
        device.stabilizationSeconds > 3600 || device.cooldownSeconds > 86400 ||
        device.maximumRecoveriesPerHour > 20) {
      setError(error, errorSize, "INVALID_POWERED_DEVICE"); return false;
    }
    if (device.enabled && channels[device.powerChannel]) {
      setError(error, errorSize, "DUPLICATE_POWER_CHANNEL"); return false;
    }
    if (device.enabled) channels[device.powerChannel] = true;
  }
  for (const DiagnosticTarget& target : model.targets) {
    if (!target.enabled) continue;
    if (!validLabel(target.name, sizeof(target.name)) || static_cast<uint8_t>(target.icon)>static_cast<uint8_t>(ObjectIcon::Website) ||
        static_cast<uint8_t>(target.test)>static_cast<uint8_t>(DiagnosticTest::Dns) || !validAddress(target.address) || target.timeoutMs < 100 ||
        target.timeoutMs > 10000 ||
        ((target.test == DiagnosticTest::Http || target.test == DiagnosticTest::Https) &&
          (!validHttpPath(target.path) || target.expectedHttpCode < 100 || target.expectedHttpCode > 599))) {
      setError(error, errorSize, "INVALID_DIAGNOSTIC_TARGET"); return false;
    }
  }
  for (const AutomationRule& rule : model.rules) {
    if (!rule.enabled) continue;
    if (!validLabel(rule.name, sizeof(rule.name)) || static_cast<uint8_t>(rule.conditionOperator)>1 || rule.conditionCount < 1 || rule.conditionCount > kRuleConditionCount ||
        rule.consecutiveCycles < 1 || rule.consecutiveCycles > 10 ||
        rule.poweredDeviceIndex >= kPoweredDeviceCount || !model.devices[rule.poweredDeviceIndex].enabled) {
      setError(error, errorSize, "INVALID_AUTOMATION_RULE"); return false;
    }
    for (uint8_t i = 0; i < rule.conditionCount; ++i)
      if (rule.conditions[i].targetIndex >= kDiagnosticTargetCount || static_cast<uint8_t>(rule.conditions[i].expectedState)>1 ||
          !model.targets[rule.conditions[i].targetIndex].enabled) {
        setError(error, errorSize, "INVALID_RULE_TARGET"); return false;
      }
  }
  setError(error, errorSize, "OK");
  return true;
}

void LogicModelStore::begin() {
  preferences_.begin("ng-logic", false);
  model_ = LogicModelValidator::defaults();
  static StoredLogicModel slotA{}, slotB{};
  const auto read=[this](const char* key, StoredLogicModel& value) {
    return preferences_.getBytesLength(key)==sizeof(value) &&
      preferences_.getBytes(key,&value,sizeof(value))==sizeof(value) && validStored(value);
  };
  const bool hasA=read("logic_a",slotA),hasB=read("logic_b",slotB);
  const bool useA=hasA&&(!hasB||slotA.sequence>=slotB.sequence);
  const StoredLogicModel* selected=useA?&slotA:(hasB?&slotB:nullptr);
  char error[48];
  if(selected&&LogicModelValidator::validate(selected->model,error,sizeof(error))) {
    model_=selected->model;sequence_=selected->sequence;activeSlotA_=useA;
  }
}

LogicModel LogicModelStore::snapshot() const {
  portENTER_CRITICAL(&mux_); LogicModel copy=model_; portEXIT_CRITICAL(&mux_); return copy;
}
uint32_t LogicModelStore::sequence() const {portENTER_CRITICAL(&mux_);const uint32_t value=sequence_;portEXIT_CRITICAL(&mux_);return value;}

bool LogicModelStore::save(const LogicModel& model) {
  static StoredLogicModel stored{},verified{};
  stored.magic=kLogicMagic;stored.sequence=sequence_+1;stored.model=model;
  stored.crc=crc32(reinterpret_cast<const uint8_t*>(&stored),offsetof(StoredLogicModel,crc));
  const bool writeA=!activeSlotA_;const char* key=writeA?"logic_a":"logic_b";
  if(preferences_.putBytes(key,&stored,sizeof(stored))!=sizeof(stored))return false;
  if(preferences_.getBytes(key,&verified,sizeof(verified))!=sizeof(verified)||!validStored(verified)||verified.sequence!=stored.sequence)return false;
  sequence_=stored.sequence;activeSlotA_=writeA;return true;
}

bool LogicModelStore::update(const LogicModel& model,char* error,size_t errorSize) {
  if(!LogicModelValidator::validate(model,error,errorSize))return false;
  if(!save(model)){setError(error,errorSize,"NVS_WRITE_FAILED");return false;}
  portENTER_CRITICAL(&mux_);model_=model;portEXIT_CRITICAL(&mux_);setError(error,errorSize,"OK");return true;
}
