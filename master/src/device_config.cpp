#include "device_config.h"

#include <stddef.h>

namespace {
constexpr uint32_t kConfigMagic = 0x4E475359;  // NGSY
struct StoredConfig { uint32_t magic; uint32_t sequence; DeviceConfig config; uint32_t crc; };

uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffffU;
  while (length--) { crc ^= *data++; for (uint8_t bit=0; bit<8; ++bit) crc=(crc>>1)^(0xedb88320U&(0U-(crc&1U))); }
  return ~crc;
}
bool validStored(const StoredConfig& value) {
  return value.magic==kConfigMagic && value.config.schemaVersion==1 &&
         value.crc==crc32(reinterpret_cast<const uint8_t*>(&value),offsetof(StoredConfig,crc));
}
bool validIp(const char* value) { IPAddress address; return address.fromString(value); }
uint32_t ipValue(const IPAddress& address) {
  return (static_cast<uint32_t>(address[0]) << 24) |
         (static_cast<uint32_t>(address[1]) << 16) |
         (static_cast<uint32_t>(address[2]) << 8) | address[3];
}
bool validStaticNetwork(const DeviceConfig& value) {
  IPAddress ip,mask,gateway,dns;
  if(!ip.fromString(value.deviceIp)||!mask.fromString(value.subnetMask)||
     !gateway.fromString(value.deviceGateway)||!dns.fromString(value.primaryDns))return false;
  const uint32_t i=ipValue(ip),m=ipValue(mask),g=ipValue(gateway),d=ipValue(dns);
  const uint32_t inverted=~m;
  if(!i||i==0xffffffffU||!g||!d||!m||m==0xffffffffU)return false;
  if((inverted&(inverted+1U))!=0)return false;
  if((i&m)!=(g&m))return false;
  const uint32_t host=i&inverted;
  return host!=0&&host!=inverted;
}
void setError(char* output,size_t size,const char* message){if(size)strlcpy(output,message,size);}
}  // namespace

DeviceConfig DeviceConfigStore::defaults(){return DeviceConfig{};}

void DeviceConfigStore::begin(){
  preferences_.begin("ng-system",false);config_=defaults();static StoredConfig a{},b{};
  const auto read=[this](const char* key,StoredConfig& value){return preferences_.getBytesLength(key)==sizeof(value)&&preferences_.getBytes(key,&value,sizeof(value))==sizeof(value)&&validStored(value);};
  const bool hasA=read("system_a",a),hasB=read("system_b",b),useA=hasA&&(!hasB||a.sequence>=b.sequence);
  const StoredConfig* selected=useA?&a:(hasB?&b:nullptr);char error[48];
  if(selected&&validate(selected->config,error,sizeof(error))){config_=selected->config;sequence_=selected->sequence;activeSlotA_=useA;}
}

DeviceConfig DeviceConfigStore::snapshot()const{portENTER_CRITICAL(&mux_);const DeviceConfig copy=config_;portEXIT_CRITICAL(&mux_);return copy;}
uint32_t DeviceConfigStore::sequence()const{portENTER_CRITICAL(&mux_);const uint32_t value=sequence_;portEXIT_CRITICAL(&mux_);return value;}

bool DeviceConfigStore::validate(const DeviceConfig& value,char* error,size_t errorSize){
  if(value.schemaVersion!=1){setError(error,errorSize,"INVALID_SYSTEM_SCHEMA");return false;}
  if(!value.useDhcp&&!validStaticNetwork(value)){setError(error,errorSize,"INVALID_STATIC_NETWORK");return false;}
  if(value.diagnosticIntervalSeconds<5||value.diagnosticIntervalSeconds>300){setError(error,errorSize,"INVALID_DIAGNOSTIC_INTERVAL");return false;}
  if(value.startupDelaySeconds>3600){setError(error,errorSize,"INVALID_STARTUP_DELAY");return false;}
  const size_t passwordLength=strnlen(value.webPassword,sizeof(value.webPassword));
  if(passwordLength<5||passwordLength>32){setError(error,errorSize,"INVALID_WEB_PASSWORD");return false;}
  setError(error,errorSize,"OK");return true;
}

bool DeviceConfigStore::save(const DeviceConfig& value){
  static StoredConfig stored{},verified{};stored.magic=kConfigMagic;stored.sequence=sequence_+1;stored.config=value;
  stored.crc=crc32(reinterpret_cast<const uint8_t*>(&stored),offsetof(StoredConfig,crc));const bool writeA=!activeSlotA_;const char* key=writeA?"system_a":"system_b";
  if(preferences_.putBytes(key,&stored,sizeof(stored))!=sizeof(stored))return false;
  if(preferences_.getBytes(key,&verified,sizeof(verified))!=sizeof(verified)||!validStored(verified)||verified.sequence!=stored.sequence)return false;
  sequence_=stored.sequence;activeSlotA_=writeA;return true;
}

bool DeviceConfigStore::update(const DeviceConfig& value,char* error,size_t errorSize){
  if(!validate(value,error,errorSize))return false;
  if(strcmp(value.webPassword,config_.webPassword)&&strnlen(value.webPassword,sizeof(value.webPassword))<8){setError(error,errorSize,"WEAK_WEB_PASSWORD");return false;}
  if(!strcmp(value.webPassword,"admin")&&strcmp(config_.webPassword,"admin")){setError(error,errorSize,"INSECURE_WEB_PASSWORD");return false;}
  if(!save(value)){setError(error,errorSize,"NVS_WRITE_FAILED");return false;}
  portENTER_CRITICAL(&mux_);config_=value;portEXIT_CRITICAL(&mux_);setError(error,errorSize,"OK");return true;
}
