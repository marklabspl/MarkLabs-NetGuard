#include <Arduino.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "network_monitor.h"
#include "device_config.h"
#include "power_module.h"
#include "status_server.h"
#include "logic_model.h"
#include "logic_engine.h"
#include "home_assistant_config.h"
#include "home_assistant_mqtt.h"

HardwareSerial powerSerial(2);
DeviceConfigStore deviceConfig;
NetworkMonitor networkMonitor(deviceConfig);
PowerModule powerModule(powerSerial);
LogicModelStore logicModel;
LogicEngine logicEngine(logicModel,deviceConfig,powerModule);
HomeAssistantConfigStore homeAssistantConfig;
HomeAssistantMqtt homeAssistant(homeAssistantConfig,deviceConfig,logicModel,logicEngine,networkMonitor,powerModule);
StatusServer statusServer(networkMonitor,powerModule,deviceConfig,logicModel,logicEngine,homeAssistantConfig,homeAssistant);

namespace {
char consoleLine[64];
size_t consoleLength = 0;

void printNet() {
  Serial.printf("NET LINK=%s IP=%s MAC=%s CHANGES=%lu\n",
                networkMonitor.linkUp() ? "UP" : "DOWN", networkMonitor.ipAddress(),
                networkMonitor.macAddress(),
                static_cast<unsigned long>(networkMonitor.linkChanges()));
}

void printPower() {
  const uint32_t age = powerModule.lastResponseAge(millis());
  Serial.printf("POWER ONLINE=%u AGE_MS=", powerModule.online() ? 1 : 0);
  if (age == UINT32_MAX) Serial.print("NONE"); else Serial.print(age);
  Serial.printf(" HEARTBEATS=%lu TIMEOUTS=%lu INVALID=%lu\n",
                static_cast<unsigned long>(powerModule.successfulHeartbeats()),
                static_cast<unsigned long>(powerModule.timeouts()),
                static_cast<unsigned long>(powerModule.invalidLines()));
}

void printDiag(){const LogicEngineSnapshot d=logicEngine.snapshot();Serial.printf("LOGIC VALID=%u RUNNING=%u RUNS=%lu REQUESTED=%lu EXECUTED=%lu FAILED=%lu DECISION=%s\n",d.valid,d.running,static_cast<unsigned long>(d.completedRuns),static_cast<unsigned long>(d.requested),static_cast<unsigned long>(d.executed),static_cast<unsigned long>(d.failed),d.lastDecision);}
void printMqtt(){const HomeAssistantMqttStatus s=homeAssistant.status();Serial.printf("MQTT ENABLED=%u CONNECTED=%u STATE=%d RECONNECTS=%lu PUBLISHED=%lu RECEIVED=%lu\n",s.enabled,s.connected,s.state,static_cast<unsigned long>(s.reconnects),static_cast<unsigned long>(s.published),static_cast<unsigned long>(s.received));}

void executeConsole() {
  consoleLine[consoleLength] = '\0';
  for (size_t i = 0; i < consoleLength; ++i)
    consoleLine[i] = static_cast<char>(toupper(static_cast<unsigned char>(consoleLine[i])));
  if (!strcmp(consoleLine, "STATUS")) { printNet(); printPower(); printDiag(); printMqtt(); }
  else if (!strcmp(consoleLine, "NET")) printNet();
  else if (!strcmp(consoleLine, "POWER")) printPower();
  else if (!strcmp(consoleLine, "DIAG")) printDiag();
  else if (!strcmp(consoleLine, "MQTT")) printMqtt();
  else if (!strcmp(consoleLine, "HELP")) Serial.println("OK HELP STATUS NET POWER DIAG MQTT HELP");
  else if (consoleLength) Serial.println("ERR COMMAND");
  consoleLength = 0;
}

void serviceConsole() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') { if (consoleLength) executeConsole(); continue; }
    if (consoleLength + 1 < sizeof(consoleLine)) consoleLine[consoleLength++] = c;
    else { consoleLength = 0; Serial.println("ERR LINE TOO LONG"); }
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.printf("\nBOOT %s %s %s\n", config::kProduct, config::kVersion,
                config::kOperatingMode);

  esp_task_wdt_init(8, true);
  esp_task_wdt_add(nullptr);

  powerModule.begin(config::kPowerUartRxPin, config::kPowerUartTxPin,
                    config::kPowerUartBaud);
  deviceConfig.begin();
  logicModel.begin();
  homeAssistantConfig.begin();
  networkMonitor.begin();
  logicEngine.begin();
  homeAssistant.begin();
  statusServer.begin();
}

void loop() {
  const uint32_t now = millis();
  networkMonitor.update(now);
  logicEngine.setNetworkReady(networkMonitor.hasIp());
  powerModule.update(now);
  logicEngine.update(now);
  homeAssistant.update(now);
  serviceConsole();
  statusServer.update();
  esp_task_wdt_reset();
  delay(1);
}
