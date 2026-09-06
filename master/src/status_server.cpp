#include "status_server.h"

#include <cstring>
#include <Update.h>
#include <esp_system.h>
#include "config.h"
#include "device_config.h"
#include "logic_engine.h"
#include "logic_model.h"
#include "network_monitor.h"
#include "power_module.h"
#include "web_config_page.h"
#include "web_dashboard_page.h"
#include "web_logic_page.h"
#include "web_update_page.h"
#include "web_backup_page.h"
#include "web_home_assistant_page.h"
#include "web_theme.h"
#include "home_assistant_config.h"
#include "home_assistant_mqtt.h"

namespace {
String escaped(const char* value){String s(value);s.replace("\\","\\\\");s.replace("\"","\\\"");s.replace("\r","\\r");s.replace("\n","\\n");return s;}
String themedPage(const char* source){String page=FPSTR(source);page.replace("MARK<span>LABS</span>","MARK<span>LABS</span><small class=brandDomain>.PL</small>");page.replace("</head>",String(FPSTR(web::kSharedTheme))+"</head>");return page;}
const char* resetReason(){switch(esp_reset_reason()){case ESP_RST_POWERON:return "POWER_ON";case ESP_RST_EXT:return "EXTERNAL";case ESP_RST_SW:return "SOFTWARE";case ESP_RST_PANIC:return "PANIC";case ESP_RST_INT_WDT:return "INTERRUPT_WDT";case ESP_RST_TASK_WDT:return "TASK_WDT";case ESP_RST_WDT:return "WATCHDOG";case ESP_RST_DEEPSLEEP:return "DEEP_SLEEP";case ESP_RST_BROWNOUT:return "BROWNOUT";case ESP_RST_SDIO:return "SDIO";default:return "UNKNOWN";}}
}

StatusServer::StatusServer(NetworkMonitor& network,PowerModule& power,DeviceConfigStore& configStore,LogicModelStore& logic,LogicEngine& logicEngine,HomeAssistantConfigStore& homeAssistantConfig,HomeAssistantMqtt& homeAssistant)
  :server_(config::kHttpPort),network_(network),power_(power),config_(configStore),logic_(logic),logicEngine_(logicEngine),homeAssistantConfig_(homeAssistantConfig),homeAssistant_(homeAssistant){}

void StatusServer::begin(){
  static const char* headers[]={"Origin"};server_.collectHeaders(headers,1);
  server_.on("/",HTTP_GET,[this](){handleRoot();});
  server_.on("/status",HTTP_GET,[this](){handleStatus();});server_.on("/api/v1/status",HTTP_GET,[this](){handleStatus();});
  server_.on("/config",HTTP_GET,[this](){handleConfigPage();});server_.on("/api/v1/config",HTTP_GET,[this](){handleConfigGet();});server_.on("/api/v1/config",HTTP_POST,[this](){handleConfigPost();});
  server_.on("/backup",HTTP_GET,[this](){handleBackupPage();});
  server_.on("/home-assistant",HTTP_GET,[this](){handleHomeAssistantPage();});
  server_.on("/api/v1/home-assistant",HTTP_GET,[this](){handleHomeAssistantGet();});
  server_.on("/api/v1/home-assistant",HTTP_POST,[this](){handleHomeAssistantPost();});
  server_.on("/api/v1/home-assistant/status",HTTP_GET,[this](){handleHomeAssistantStatus();});
  server_.on("/logic",HTTP_GET,[this](){handleLogicPage();});server_.on("/api/v1/logic",HTTP_GET,[this](){handleLogicGet();});server_.on("/api/v1/logic",HTTP_POST,[this](){handleLogicPost();});server_.on("/api/v1/logic/status",HTTP_GET,[this](){handleLogicStatus();});
  server_.on("/api/v1/test",HTTP_POST,[this](){handleManualTest();});server_.on("/api/v1/automation",HTTP_POST,[this](){handleAutomationPost();});
  server_.on("/api/v1/devices/reset",HTTP_POST,[this](){handleManualPulse();});
  server_.on("/api/v1/devices/pulse",HTTP_POST,[this](){handleManualPulse();});  // Legacy API alias.
  server_.on("/api/v1/system/events",HTTP_GET,[this](){handleSystemEvents();});
  server_.on("/diagnostics",HTTP_GET,[this](){server_.sendHeader("Location","/");server_.send(302,"text/plain","");});
  server_.on("/api/v1/restart",HTTP_POST,[this](){handleRestart();});
  server_.on("/update",HTTP_GET,[this](){handleUpdatePage();});server_.on("/update",HTTP_POST,[this](){handleUpdateFinished();},[this](){handleUpdateUpload();});
  server_.onNotFound([this](){server_.send(404,"text/plain","Not found\n");});server_.begin();addSystemEvent("BOOT",true);
}

bool StatusServer::requireAuthentication(){
  const uint32_t now=millis();const IPAddress remote=server_.client().remoteIP();LoginAttempt* attempt=nullptr;LoginAttempt* oldest=&loginAttempts_[0];
  for(auto& item:loginAttempts_){if(item.address==remote){attempt=&item;break;}if(!item.lastSeen||item.lastSeen<oldest->lastSeen)oldest=&item;}
  if(!attempt){attempt=oldest;*attempt=LoginAttempt{};attempt->address=remote;}attempt->lastSeen=now;
  if(attempt->blockedUntil&&static_cast<int32_t>(attempt->blockedUntil-now)>0){server_.send(429,"text/plain","Login temporarily blocked\n");return false;}
  const DeviceConfig current=config_.snapshot();if(server_.authenticate(config::kWebAdminUser,current.webPassword)){attempt->failures=0;attempt->blockedUntil=0;return true;}
  if(++attempt->failures>=5){attempt->blockedUntil=now+60000U;attempt->failures=0;server_.send(429,"text/plain","Login blocked for 60 seconds\n");return false;}
  server_.requestAuthentication(BASIC_AUTH,"MarkLabs NetGuard");return false;
}
bool StatusServer::requireSameOrigin(){if(!server_.hasHeader("Origin")){server_.send(403,"text/plain","Origin required\n");return false;}const String expected="http://"+server_.hostHeader();if(server_.header("Origin")==expected)return true;server_.send(403,"text/plain","Origin rejected\n");return false;}
bool StatusServer::requireChangedPassword(){if(strcmp(config_.snapshot().webPassword,config::kWebAdminPassword))return true;server_.send(428,"application/json","{\"ok\":false,\"error\":\"PASSWORD_CHANGE_REQUIRED\"}");return false;}
void StatusServer::sendSecurityHeaders(){server_.sendHeader("X-Frame-Options","DENY");server_.sendHeader("X-Content-Type-Options","nosniff");server_.sendHeader("Referrer-Policy","no-referrer");server_.sendHeader("Content-Security-Policy","frame-ancestors 'none'; base-uri 'self'; form-action 'self'");}

void StatusServer::handleRoot(){if(!requireAuthentication())return;sendSecurityHeaders();server_.sendHeader("Cache-Control","no-store");server_.send(200,"text/html",themedPage(web::kDashboardPage));}
void StatusServer::handleConfigPage(){
  if(!requireAuthentication())return;
  String page=themedPage(web::kConfigPage);
  if(!strcmp(config_.snapshot().webPassword,config::kWebAdminPassword))page.replace("<form","<div class=notice style=\"border:1px solid #f59e0b;background:#fffbeb;color:#92400e;padding:14px;border-radius:12px;margin-bottom:16px\"><strong>Wymagana zmiana hasła / Password change required</strong><br>Ustaw nowe hasło o długości co najmniej 8 znaków, aby odblokować zapis logiki, RESET, restart i OTA.<br>Set a new password of at least 8 characters to unlock logic changes, RESET, restart and OTA.</div><form");
  page.replace("</form>","<button class=save type=button style=\"background:#334155;margin-left:8px\" onclick=\"this.disabled=true;fetch('/api/v1/restart',{method:'POST'}).then(r=>{if(!r.ok)throw Error(r.status);setTimeout(()=>location.href='/',5000)}).catch(()=>{this.disabled=false})\">Restart / Uruchom ponownie</button></form>");
  sendSecurityHeaders();server_.sendHeader("Cache-Control","no-store");
  server_.send(200,"text/html",page);
}
void StatusServer::handleBackupPage(){if(!requireAuthentication())return;sendSecurityHeaders();server_.sendHeader("Cache-Control","no-store");server_.send(200,"text/html",themedPage(web::kBackupPage));}
void StatusServer::handleHomeAssistantPage(){if(!requireAuthentication())return;sendSecurityHeaders();server_.sendHeader("Cache-Control","no-store");server_.send(200,"text/html",themedPage(web::kHomeAssistantPage));}

void StatusServer::handleHomeAssistantGet(){
  if(!requireAuthentication())return;
  const HomeAssistantConfig c=homeAssistantConfig_.snapshot();
  String json=String("{\"enabled\":")+(c.enabled?"true":"false")+",\"tls\":"+(c.tls?"true":"false")+",\"broker\":\""+escaped(c.broker)+"\",\"port\":"+c.port+",\"username\":\""+escaped(c.username)+"\",\"has_password\":"+(c.password[0]?"true":"false")+",\"device_name\":\""+escaped(c.deviceName)+"\",\"discovery_prefix\":\""+escaped(c.discoveryPrefix)+"\",\"topic_prefix\":\""+escaped(c.topicPrefix)+"\",\"language\":\""+escaped(c.language)+"\"}";
  server_.sendHeader("Cache-Control","no-store");server_.send(200,"application/json",json);
}

void StatusServer::handleHomeAssistantPost(){
  if(!requireAuthentication()||!requireSameOrigin()||!requireChangedPassword())return;
  HomeAssistantConfig c=homeAssistantConfig_.snapshot();
  const auto copy=[this](const char* key,char* output,size_t size){if(server_.hasArg(key))strlcpy(output,server_.arg(key).c_str(),size);};
  c.enabled=server_.hasArg("enabled");c.tls=server_.hasArg("tls");
  copy("broker",c.broker,sizeof(c.broker));copy("username",c.username,sizeof(c.username));copy("device_name",c.deviceName,sizeof(c.deviceName));copy("discovery_prefix",c.discoveryPrefix,sizeof(c.discoveryPrefix));copy("topic_prefix",c.topicPrefix,sizeof(c.topicPrefix));copy("language",c.language,sizeof(c.language));
  if(server_.hasArg("port")){const long port=server_.arg("port").toInt();if(port<1||port>65535){server_.send(400,"application/json","{\"ok\":false,\"error\":\"INVALID_HA_CONFIG\"}");return;}c.port=static_cast<uint16_t>(port);}
  if(server_.hasArg("password")&&server_.arg("password").length())copy("password",c.password,sizeof(c.password));
  if(server_.hasArg("clear_password"))c.password[0]='\0';
  char error[48];if(!HomeAssistantConfigStore::validate(c,error,sizeof(error))){server_.send(400,"application/json",String("{\"ok\":false,\"error\":\"")+error+"\"}");return;}homeAssistant_.beforeConfigurationChange();if(!homeAssistantConfig_.update(c,error,sizeof(error))){Serial.printf("HA CONFIG SAVE FAIL %s\n",error);homeAssistant_.configurationChanged();server_.send(400,"application/json",String("{\"ok\":false,\"error\":\"")+error+"\"}");return;}
  homeAssistant_.configurationChanged();Serial.printf("HA CONFIG SAVE OK SEQUENCE=%lu\n",static_cast<unsigned long>(homeAssistantConfig_.sequence()));server_.send(200,"application/json",String("{\"ok\":true,\"sequence\":")+homeAssistantConfig_.sequence()+"}");
}

void StatusServer::handleHomeAssistantStatus(){
  if(!requireAuthentication())return;const HomeAssistantMqttStatus s=homeAssistant_.status();
  String json=String("{\"enabled\":")+(s.enabled?"true":"false")+",\"connected\":"+(s.connected?"true":"false")+",\"state\":"+String(s.state)+",\"reconnects\":"+s.reconnects+",\"published\":"+s.published+",\"received\":"+s.received+"}";
  server_.sendHeader("Cache-Control","no-store");server_.send(200,"application/json",json);
}
void StatusServer::handleLogicPage(){if(!requireAuthentication())return;sendSecurityHeaders();server_.sendHeader("Cache-Control","no-store");server_.send(200,"text/html",themedPage(web::kLogicPage));}

void StatusServer::handleStatus(){
  if(!requireAuthentication())return;const uint32_t age=power_.lastResponseAge(millis());static char json[1400];
  const DeviceConfig settings=config_.snapshot();
  const String safePowerStatus=escaped(power_.lastStatus());
  snprintf(json,sizeof(json),"{\"product\":\"%s\",\"version\":\"%s\",\"mode\":\"%s\",\"system\":{\"uptime_ms\":%lu,\"free_heap\":%u,\"min_free_heap\":%u,\"automation_enabled\":%s,\"startup_delay_seconds\":%u,\"reset_reason\":\"%s\"},\"ethernet\":{\"link\":%s,\"has_ip\":%s,\"ip\":\"%s\",\"mac\":\"%s\",\"link_changes\":%lu,\"speed_mbps\":%u,\"full_duplex\":%s},\"power_module\":{\"online\":%s,\"last_response_ms\":%lu,\"heartbeats\":%lu,\"consecutive_heartbeats\":%u,\"timeouts\":%lu,\"invalid_lines\":%lu,\"command_pending\":%s,\"last_status\":\"%s\",\"accepted_pulses\":%lu,\"command_failures\":%lu,\"session_id\":%lu,\"module_restarts\":%lu,\"relay_states\":\"%s\"}}",
    config::kProduct,config::kVersion,config::kOperatingMode,static_cast<unsigned long>(millis()),ESP.getFreeHeap(),ESP.getMinFreeHeap(),settings.automationEnabled?"true":"false",settings.startupDelaySeconds,resetReason(),network_.linkUp()?"true":"false",network_.hasIp()?"true":"false",network_.ipAddress(),network_.macAddress(),static_cast<unsigned long>(network_.linkChanges()),network_.linkSpeedMbps(),network_.fullDuplex()?"true":"false",power_.online()?"true":"false",static_cast<unsigned long>(age),static_cast<unsigned long>(power_.successfulHeartbeats()),power_.consecutiveHeartbeats(),static_cast<unsigned long>(power_.timeouts()),static_cast<unsigned long>(power_.invalidLines()),power_.commandPending()?"true":"false",safePowerStatus.c_str(),static_cast<unsigned long>(power_.acceptedPulses()),static_cast<unsigned long>(power_.commandFailures()),static_cast<unsigned long>(power_.sessionId()),static_cast<unsigned long>(power_.moduleRestarts()),power_.relayStates());
  server_.sendHeader("Cache-Control","no-store");server_.send(200,"application/json",json);
}

void StatusServer::handleConfigGet(){
  if(!requireAuthentication())return;const DeviceConfig c=config_.snapshot();static char json[400];snprintf(json,sizeof(json),"{\"use_dhcp\":%s,\"device_ip\":\"%s\",\"subnet\":\"%s\",\"device_gateway\":\"%s\",\"primary_dns\":\"%s\",\"interval\":%u,\"startup_delay\":%u,\"automation_enabled\":%s}",c.useDhcp?"true":"false",c.deviceIp,c.subnetMask,c.deviceGateway,c.primaryDns,c.diagnosticIntervalSeconds,c.startupDelaySeconds,c.automationEnabled?"true":"false");server_.sendHeader("Cache-Control","no-store");server_.send(200,"application/json",json);
}
void StatusServer::handleConfigPost(){
  if(!requireAuthentication()||!requireSameOrigin())return;DeviceConfig c=config_.snapshot();const auto copy=[this](const char* key,char* dst,size_t size){if(server_.hasArg(key))strlcpy(dst,server_.arg(key).c_str(),size);};
  c.useDhcp=server_.hasArg("use_dhcp");c.automationEnabled=server_.hasArg("automation_enabled");copy("device_ip",c.deviceIp,sizeof(c.deviceIp));copy("subnet",c.subnetMask,sizeof(c.subnetMask));copy("device_gateway",c.deviceGateway,sizeof(c.deviceGateway));copy("primary_dns",c.primaryDns,sizeof(c.primaryDns));if(server_.hasArg("interval"))c.diagnosticIntervalSeconds=server_.arg("interval").toInt();if(server_.hasArg("startup_delay"))c.startupDelaySeconds=server_.arg("startup_delay").toInt();if(server_.hasArg("new_password")&&server_.arg("new_password").length())copy("new_password",c.webPassword,sizeof(c.webPassword));
  char error[48];if(!config_.update(c,error,sizeof(error))){Serial.printf("SYSTEM CONFIG SAVE FAIL %s\n",error);server_.send(400,"application/json",String("{\"ok\":false,\"error\":\"")+error+"\"}");return;}Serial.printf("SYSTEM CONFIG SAVE OK SEQUENCE=%lu\n",static_cast<unsigned long>(config_.sequence()));server_.send(200,"application/json",String("{\"ok\":true,\"sequence\":")+config_.sequence()+"}");
}

void StatusServer::handleLogicGet(){
  if(!requireAuthentication())return;const LogicModel m=logic_.snapshot();String j;j.reserve(7000);j="{\"devices\":[";
  for(uint8_t i=0;i<kPoweredDeviceCount;++i){if(i)j+=',';const PoweredDevice& d=m.devices[i];j+="{\"enabled\":"+String(d.enabled?"true":"false")+",\"name\":\""+escaped(d.name)+"\",\"icon\":"+String(static_cast<uint8_t>(d.icon))+",\"power_channel\":"+String(d.powerChannel)+",\"pulse_seconds\":"+String(d.pulseSeconds)+",\"stabilization_seconds\":"+String(d.stabilizationSeconds)+",\"cooldown_seconds\":"+String(d.cooldownSeconds)+",\"maximum_recoveries_per_hour\":"+String(d.maximumRecoveriesPerHour)+"}";}
  j+="],\"targets\":[";for(uint8_t i=0;i<kDiagnosticTargetCount;++i){if(i)j+=',';const DiagnosticTarget& t=m.targets[i];j+="{\"enabled\":"+String(t.enabled?"true":"false")+",\"name\":\""+escaped(t.name)+"\",\"icon\":"+String(static_cast<uint8_t>(t.icon))+",\"address\":\""+escaped(t.address)+"\",\"test\":"+String(static_cast<uint8_t>(t.test))+",\"port\":"+String(t.port)+",\"path\":\""+escaped(t.path)+"\",\"expected_http_code\":"+String(t.expectedHttpCode)+",\"timeout_ms\":"+String(t.timeoutMs)+"}";}
  j+="],\"rules\":[";for(uint8_t i=0;i<kAutomationRuleCount;++i){if(i)j+=',';const AutomationRule& r=m.rules[i];j+="{\"enabled\":"+String(r.enabled?"true":"false")+",\"name\":\""+escaped(r.name)+"\",\"operator\":"+String(static_cast<uint8_t>(r.conditionOperator))+",\"condition_count\":"+String(r.conditionCount)+",\"consecutive_cycles\":"+String(r.consecutiveCycles)+",\"powered_device_index\":"+String(r.poweredDeviceIndex)+",\"conditions\":[";for(uint8_t c=0;c<kRuleConditionCount;++c){if(c)j+=',';j+="{\"target_index\":"+String(r.conditions[c].targetIndex)+",\"expected_state\":"+String(static_cast<uint8_t>(r.conditions[c].expectedState))+"}";}j+="]}";}j+="]}";server_.sendHeader("Cache-Control","no-store");server_.send(200,"application/json",j);
}

void StatusServer::handleLogicPost(){
  if(!requireAuthentication()||!requireSameOrigin()||!requireChangedPassword())return;
  if(server_.args()>=272){Serial.printf("LOGIC SAVE FAIL FORM_DATA_TRUNCATED ARGS=%d\n",server_.args());server_.send(413,"application/json","{\"ok\":false,\"error\":\"FORM_DATA_TRUNCATED\"}");return;}
  if(logicEngine_.snapshot().pulsePending){server_.send(409,"application/json","{\"ok\":false,\"error\":\"RESET_IN_PROGRESS\"}");return;}LogicModel m=logic_.snapshot();const auto integer=[this](const String& key,long fallback){return server_.hasArg(key)?server_.arg(key).toInt():fallback;};const auto copy=[this](const String& key,char* output,size_t size){if(server_.hasArg(key))strlcpy(output,server_.arg(key).c_str(),size);};
  for(uint8_t i=0;i<kPoweredDeviceCount;++i){const String p="d"+String(i)+"_";PoweredDevice& d=m.devices[i];d.enabled=server_.hasArg(p+"enabled");copy(p+"name",d.name,sizeof(d.name));d.icon=static_cast<ObjectIcon>(integer(p+"icon",0));d.powerChannel=integer(p+"channel",i);d.pulseSeconds=integer(p+"pulse",10);d.stabilizationSeconds=integer(p+"stabilization",60);d.cooldownSeconds=integer(p+"cooldown",300);d.maximumRecoveriesPerHour=integer(p+"limit",3);}
  for(uint8_t i=0;i<kDiagnosticTargetCount;++i){const String p="t"+String(i)+"_";DiagnosticTarget& t=m.targets[i];t.enabled=server_.hasArg(p+"enabled");copy(p+"name",t.name,sizeof(t.name));copy(p+"address",t.address,sizeof(t.address));copy(p+"path",t.path,sizeof(t.path));t.icon=static_cast<ObjectIcon>(integer(p+"icon",0));t.test=static_cast<DiagnosticTest>(integer(p+"test",0));t.port=integer(p+"port",0);t.timeoutMs=integer(p+"timeout",1000);t.expectedHttpCode=integer(p+"code",200);}
  for(uint8_t i=0;i<kAutomationRuleCount;++i){const String p="r"+String(i)+"_";AutomationRule& r=m.rules[i];r.enabled=server_.hasArg(p+"enabled");copy(p+"name",r.name,sizeof(r.name));r.conditionOperator=static_cast<ConditionOperator>(integer(p+"operator",0));r.conditionCount=integer(p+"count",1);r.consecutiveCycles=integer(p+"cycles",3);r.poweredDeviceIndex=integer(p+"device",0);for(uint8_t c=0;c<kRuleConditionCount;++c){r.conditions[c].targetIndex=integer(p+"c"+String(c)+"_target",0);r.conditions[c].expectedState=static_cast<ExpectedTargetState>(integer(p+"c"+String(c)+"_state",0));}}
  char error[48];if(!logic_.update(m,error,sizeof(error))){Serial.printf("LOGIC SAVE FAIL %s ARGS=%d\n",error,server_.args());server_.send(400,"application/json",String("{\"ok\":false,\"error\":\"")+error+"\"}");return;}Serial.printf("LOGIC SAVE OK SEQUENCE=%lu ARGS=%d\n",static_cast<unsigned long>(logic_.sequence()),server_.args());server_.send(200,"application/json",String("{\"ok\":true,\"sequence\":")+logic_.sequence()+",\"received_fields\":"+server_.args()+"}");
}

void StatusServer::handleLogicStatus(){
  if(!requireAuthentication())return;const LogicEngineSnapshot s=logicEngine_.snapshot();String j;j.reserve(3000);
  j="{\"valid\":"+String(s.valid?"true":"false")+",\"running\":"+String(s.running?"true":"false")+",\"operations_inhibited\":"+String(s.operationsInhibited?"true":"false")+",\"completed_runs\":"+String(s.completedRuns)+",\"requested\":"+String(s.requested)+",\"executed\":"+String(s.executed)+",\"failed\":"+String(s.failed)+",\"pulse_pending\":"+String(s.pulsePending?"true":"false")+",\"pending_device\":"+String(s.pendingDevice)+",\"last_decision_at_ms\":"+String(s.lastDecisionAtMs)+",\"last_decision\":\""+escaped(s.lastDecision)+"\",\"targets\":[";
  for(uint8_t i=0;i<kDiagnosticTargetCount;++i){if(i)j+=',';const LogicTargetResult& t=s.targets[i];j+="{\"enabled\":"+String(t.enabled?"true":"false")+",\"ok\":"+String(t.ok?"true":"false")+",\"response_ms\":"+String(t.responseMs,1)+",\"response_code\":"+String(t.responseCode)+",\"checks\":"+String(t.checks)+",\"successes\":"+String(t.successes)+"}";}
  j+="],\"devices\":[";for(uint8_t i=0;i<kPoweredDeviceCount;++i){if(i)j+=',';const LogicDeviceRuntime& d=s.devices[i];j+="{\"stabilization_remaining_seconds\":"+String(d.stabilizationRemainingSeconds)+",\"cooldown_remaining_seconds\":"+String(d.cooldownRemainingSeconds)+",\"recoveries_in_current_hour\":"+String(d.recoveriesInCurrentHour)+"}";}
  j+="],\"rules\":[";for(uint8_t i=0;i<kAutomationRuleCount;++i){if(i)j+=',';const LogicRuleRuntime& r=s.rules[i];j+="{\"enabled\":"+String(r.enabled?"true":"false")+",\"matched\":"+String(r.matched?"true":"false")+",\"matching_cycles\":"+String(r.matchingCycles)+",\"required_cycles\":"+String(r.requiredCycles)+",\"state\":"+String(static_cast<uint8_t>(r.state))+"}";}
  j+="],\"events\":[";for(uint8_t i=0;i<s.eventCount;++i){if(i)j+=',';j+="{\"timestamp_ms\":"+String(s.events[i].timestampMs)+",\"message\":\""+escaped(s.events[i].message)+"\"}";}
  j+="]}";server_.sendHeader("Cache-Control","no-store");server_.send(200,"application/json",j);
}

void StatusServer::handleManualTest(){
  if(!requireAuthentication()||!requireSameOrigin()||!requireChangedPassword())return;
  logicEngine_.requestRun();
  server_.send(202,"application/json","{\"ok\":true}");
}

void StatusServer::handleAutomationPost(){
  if(!requireAuthentication()||!requireSameOrigin()||!requireChangedPassword())return;
  DeviceConfig settings=config_.snapshot();
  settings.automationEnabled=server_.hasArg("enabled")&&server_.arg("enabled")!="0";
  char error[48];
  if(!config_.update(settings,error,sizeof(error))){
    server_.send(400,"application/json",String("{\"ok\":false,\"error\":\"")+error+"\"}");
    return;
  }
  addSystemEvent("AUTOMATION",settings.automationEnabled);
  server_.send(200,"application/json",String("{\"ok\":true,\"automation_enabled\":")+(settings.automationEnabled?"true":"false")+"}");
}

void StatusServer::handleSystemEvents(){
  if(!requireAuthentication())return;
  String json="{\"events\":[";json.reserve(1800);
  for(uint8_t i=0;i<systemEventCount_;++i){if(i)json+=',';const SystemEvent& event=systemEvents_[i];json+="{\"timestamp_ms\":"+String(event.timestampMs)+",\"code\":\""+String(event.code)+"\",\"state\":"+String(event.state?"true":"false")+"}";}
  json+="]}";server_.sendHeader("Cache-Control","no-store");server_.send(200,"application/json",json);
}

void StatusServer::addSystemEvent(const char* code,bool state){
  if(systemEventCount_>=24){memmove(&systemEvents_[0],&systemEvents_[1],sizeof(SystemEvent)*23);systemEventCount_=23;}
  SystemEvent& event=systemEvents_[systemEventCount_++];event.timestampMs=millis();strlcpy(event.code,code,sizeof(event.code));event.state=state;
}

void StatusServer::trackSystemEvents(){
  const bool link=network_.linkUp(),power=power_.online();
  if(!observedStateReady_){previousLink_=link;previousPower_=power;observedStateReady_=true;return;}
  if(link!=previousLink_){previousLink_=link;addSystemEvent("ETHERNET",link);}
  if(power!=previousPower_){previousPower_=power;addSystemEvent("POWER_MODULE",power);}
}

void StatusServer::handleManualPulse(){
  if(!requireAuthentication()||!requireSameOrigin()||!requireChangedPassword())return;
  if(!server_.hasArg("device")){server_.send(400,"application/json","{\"ok\":false,\"error\":\"INVALID_DEVICE\"}");return;}
  const int device=server_.arg("device").toInt();
  char error[48]="INVALID_DEVICE";
  if(device<0||device>=kPoweredDeviceCount||!logicEngine_.requestManualPulse(static_cast<uint8_t>(device),millis(),error,sizeof(error))){
    server_.send(409,"application/json",String("{\"ok\":false,\"error\":\"")+error+"\"}");return;
  }
  server_.send(202,"application/json","{\"ok\":true}");
}

void StatusServer::handleRestart(){if(!requireAuthentication()||!requireSameOrigin()||!requireChangedPassword())return;if(logicEngine_.snapshot().pulsePending||power_.commandPending()){server_.send(409,"application/json","{\"ok\":false,\"error\":\"RESET_IN_PROGRESS\"}");return;}server_.send(200,"text/plain","Restarting\n");delay(250);ESP.restart();}
void StatusServer::handleUpdatePage(){if(!requireAuthentication())return;sendSecurityHeaders();server_.sendHeader("Cache-Control","no-store");server_.send(200,"text/html",themedPage(web::kUpdatePage));}
void StatusServer::handleUpdateUpload(){
  HTTPUpload& upload=server_.upload();
  if(upload.status==UPLOAD_FILE_START){
    otaAuthenticated_=false;otaHeaderValidated_=false;otaError_[0]='\0';
    const DeviceConfig c=config_.snapshot();
    if(!server_.authenticate(config::kWebAdminUser,c.webPassword)){strlcpy(otaError_,"AUTHENTICATION_FAILED",sizeof(otaError_));return;}
    if(!strcmp(c.webPassword,config::kWebAdminPassword)){strlcpy(otaError_,"PASSWORD_CHANGE_REQUIRED",sizeof(otaError_));return;}
    if(!requireSameOrigin()){strlcpy(otaError_,"ORIGIN_REJECTED",sizeof(otaError_));return;}
    if(logicEngine_.snapshot().pulsePending||power_.commandPending()){strlcpy(otaError_,"RESET_IN_PROGRESS",sizeof(otaError_));return;}
    String filename=upload.filename;filename.toLowerCase();
    if(!filename.startsWith("netguard-master-wt32-eth01-v")||!filename.endsWith("-app.bin")){strlcpy(otaError_,"INVALID_PRODUCT_IMAGE",sizeof(otaError_));return;}
    if(!Update.begin(ESP.getFreeSketchSpace(),U_FLASH)){strlcpy(otaError_,"IMAGE_TOO_LARGE",sizeof(otaError_));return;}
    otaAuthenticated_=true;logicEngine_.setOperationsInhibited(true);addSystemEvent("OTA",true);
  }else if(otaAuthenticated_&&upload.status==UPLOAD_FILE_WRITE){
    if(!otaHeaderValidated_){if(!upload.currentSize||upload.buf[0]!=0xE9){strlcpy(otaError_,"INVALID_ESP32_IMAGE",sizeof(otaError_));Update.abort();otaAuthenticated_=false;logicEngine_.setOperationsInhibited(false);return;}otaHeaderValidated_=true;}
    if(Update.write(upload.buf,upload.currentSize)!=upload.currentSize){strlcpy(otaError_,"FLASH_WRITE_FAILED",sizeof(otaError_));Update.abort();otaAuthenticated_=false;logicEngine_.setOperationsInhibited(false);}
  }else if(otaAuthenticated_&&upload.status==UPLOAD_FILE_END){
    if(!otaHeaderValidated_||!Update.end(true)){strlcpy(otaError_,"IMAGE_VALIDATION_FAILED",sizeof(otaError_));otaAuthenticated_=false;logicEngine_.setOperationsInhibited(false);}
  }else if(upload.status==UPLOAD_FILE_ABORTED){strlcpy(otaError_,"UPLOAD_ABORTED",sizeof(otaError_));Update.abort();otaAuthenticated_=false;logicEngine_.setOperationsInhibited(false);}
}
void StatusServer::handleUpdateFinished(){
  if(!otaAuthenticated_){logicEngine_.setOperationsInhibited(false);addSystemEvent("OTA",false);server_.send(400,"text/plain",otaError_[0]?otaError_:"UPDATE_FAILED");return;}
  server_.send(200,"text/plain","UPDATE_OK");delay(300);ESP.restart();
}
void StatusServer::update(){trackSystemEvents();server_.handleClient();}
