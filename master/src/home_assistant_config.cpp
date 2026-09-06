#include "home_assistant_config.h"

#include <ctype.h>
#include <stddef.h>

namespace {
constexpr uint32_t kMagic = 0x4E474841;  // NGHA
struct StoredConfig { uint32_t magic; uint32_t sequence; HomeAssistantConfig config; uint32_t crc; };
struct HomeAssistantConfigV1 {
  uint32_t schemaVersion; bool enabled; bool tls; char broker[64]; uint16_t port;
  char username[33]; char password[65]; char deviceName[33];
  char discoveryPrefix[33]; char topicPrefix[65];
};
struct StoredConfigV1 { uint32_t magic; uint32_t sequence; HomeAssistantConfigV1 config; uint32_t crc; };

uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffffU;
  while (length--) {
    crc ^= *data++;
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}
bool validStored(const StoredConfig& value) {
  return value.magic == kMagic && value.config.schemaVersion == 2 &&
         value.crc == crc32(reinterpret_cast<const uint8_t*>(&value), offsetof(StoredConfig, crc));
}
bool validStoredV1(const StoredConfigV1& value) {
  return value.magic == kMagic && value.config.schemaVersion == 1 &&
         value.crc == crc32(reinterpret_cast<const uint8_t*>(&value), offsetof(StoredConfigV1, crc));
}
void setError(char* output, size_t size, const char* message) {
  if (size) strlcpy(output, message, size);
}
bool validText(const char* value, size_t capacity, bool allowEmpty) {
  const size_t length = strnlen(value, capacity);
  if (length >= capacity || (!allowEmpty && !length)) return false;
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (c < 0x20 || c == 0x7f || c == '<' || c == '>' || c == '"') return false;
  }
  return true;
}
bool validTopic(const char* value, size_t capacity) {
  if (!validText(value, capacity, false)) return false;
  const size_t length = strlen(value);
  if (value[0] == '/' || value[length - 1] == '/') return false;
  for (size_t i = 0; i < length; ++i)
    if (value[i] == '#' || value[i] == '+' || isspace(static_cast<unsigned char>(value[i]))) return false;
  return true;
}
bool validBroker(const char* value, size_t capacity) {
  if (!validText(value, capacity, false)) return false;
  for (const char* p = value; *p; ++p)
    if (!(isalnum(static_cast<unsigned char>(*p)) || *p == '-' || *p == '.')) return false;
  return true;
}
}  // namespace

void HomeAssistantConfigStore::begin() {
  preferences_.begin("ng-ha", false);
  static StoredConfig a{}, b{};
  const auto read = [this](const char* key, StoredConfig& value) {
    return preferences_.getBytesLength(key) == sizeof(value) &&
           preferences_.getBytes(key, &value, sizeof(value)) == sizeof(value) && validStored(value);
  };
  const bool hasA = read("ha_a", a), hasB = read("ha_b", b);
  const bool useA = hasA && (!hasB || a.sequence >= b.sequence);
  const StoredConfig* selected = useA ? &a : (hasB ? &b : nullptr);
  char error[48];
  if (selected && validate(selected->config, error, sizeof(error))) {
    config_ = selected->config;
    sequence_ = selected->sequence;
    activeSlotA_ = useA;
    return;
  }
  static StoredConfigV1 oldA{}, oldB{};
  const auto readOld = [this](const char* key, StoredConfigV1& value) {
    return preferences_.getBytesLength(key) == sizeof(value) &&
           preferences_.getBytes(key, &value, sizeof(value)) == sizeof(value) && validStoredV1(value);
  };
  const bool hasOldA = readOld("ha_a", oldA), hasOldB = readOld("ha_b", oldB);
  const bool useOldA = hasOldA && (!hasOldB || oldA.sequence >= oldB.sequence);
  const StoredConfigV1* old = useOldA ? &oldA : (hasOldB ? &oldB : nullptr);
  if (old) {
    config_.enabled = old->config.enabled; config_.tls = old->config.tls;
    config_.port = old->config.port;
    strlcpy(config_.broker, old->config.broker, sizeof(config_.broker));
    strlcpy(config_.username, old->config.username, sizeof(config_.username));
    strlcpy(config_.password, old->config.password, sizeof(config_.password));
    strlcpy(config_.deviceName, old->config.deviceName, sizeof(config_.deviceName));
    strlcpy(config_.discoveryPrefix, old->config.discoveryPrefix, sizeof(config_.discoveryPrefix));
    strlcpy(config_.topicPrefix, old->config.topicPrefix, sizeof(config_.topicPrefix));
    sequence_ = old->sequence; activeSlotA_ = useOldA;
    if (!save(config_)) Serial.println("HA CONFIG MIGRATION SAVE FAILED");
  }
}

HomeAssistantConfig HomeAssistantConfigStore::snapshot() const {
  portENTER_CRITICAL(&mux_);
  const HomeAssistantConfig copy = config_;
  portEXIT_CRITICAL(&mux_);
  return copy;
}
uint32_t HomeAssistantConfigStore::sequence() const {
  portENTER_CRITICAL(&mux_);
  const uint32_t value = sequence_;
  portEXIT_CRITICAL(&mux_);
  return value;
}

bool HomeAssistantConfigStore::validate(const HomeAssistantConfig& value, char* error, size_t size) {
  if (value.schemaVersion != 2) { setError(error, size, "INVALID_HA_SCHEMA"); return false; }
  if (value.port == 0 || !validText(value.deviceName, sizeof(value.deviceName), false) ||
      !validTopic(value.discoveryPrefix, sizeof(value.discoveryPrefix)) ||
      !validTopic(value.topicPrefix, sizeof(value.topicPrefix)) ||
      !validText(value.username, sizeof(value.username), true) ||
      !validText(value.password, sizeof(value.password), true) ||
      (value.enabled && !validBroker(value.broker, sizeof(value.broker))) ||
      (strcmp(value.language, "pl") && strcmp(value.language, "en"))) {
    setError(error, size, "INVALID_HA_CONFIG");
    return false;
  }
  setError(error, size, "OK");
  return true;
}

bool HomeAssistantConfigStore::save(const HomeAssistantConfig& value) {
  static StoredConfig stored{}, verified{};
  stored.magic = kMagic;
  stored.sequence = sequence_ + 1;
  stored.config = value;
  stored.crc = crc32(reinterpret_cast<const uint8_t*>(&stored), offsetof(StoredConfig, crc));
  const bool writeA = !activeSlotA_;
  const char* key = writeA ? "ha_a" : "ha_b";
  if (preferences_.putBytes(key, &stored, sizeof(stored)) != sizeof(stored)) return false;
  if (preferences_.getBytes(key, &verified, sizeof(verified)) != sizeof(verified) ||
      !validStored(verified) || verified.sequence != stored.sequence) return false;
  sequence_ = stored.sequence;
  activeSlotA_ = writeA;
  return true;
}

bool HomeAssistantConfigStore::update(const HomeAssistantConfig& value, char* error, size_t size) {
  if (!validate(value, error, size)) return false;
  if (!save(value)) { setError(error, size, "NVS_WRITE_FAILED"); return false; }
  portENTER_CRITICAL(&mux_);
  config_ = value;
  portEXIT_CRITICAL(&mux_);
  setError(error, size, "OK");
  return true;
}
