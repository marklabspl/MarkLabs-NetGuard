#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "protocol.h"

namespace {

constexpr char kProduct[] = "NETGUARD-POWER";
constexpr char kVersion[] = "0.3.1";
constexpr uint8_t kRelayPins[netguard::kRelayCount] = {12, 5, 4, 15};
constexpr size_t kInputCapacity = 96;
constexpr uint32_t kHeartbeatIntervalMs = 5000;
constexpr uint32_t kMinimumSwitchIntervalMs = 1000;

struct RelayState {
  bool on;
  bool timerActive;
  uint32_t deadline;
  uint32_t transactionId;
  uint32_t transactionSession;
  uint32_t lastSwitchAt;
};

RelayState relays[netguard::kRelayCount];
char inputBuffer[kInputCapacity];
size_t inputLength = 0;
bool discardingLongLine = false;
bool previousWasCr = false;
uint32_t sessionId = 0;
uint32_t nextHeartbeatAt = 0;
uint32_t lastTransactionId = 0;
uint32_t lastTransactionSession = 0;
uint8_t lastTransactionChannel = 0;
uint32_t lastTransactionSeconds = 0;

void setRelay(uint8_t index, bool on) {
  digitalWrite(kRelayPins[index], on ? HIGH : LOW);
  relays[index].on = on;
  relays[index].lastSwitchAt = millis();
  if (on) relays[index].timerActive = false;
}

bool relayReady(uint8_t index, uint32_t now) {
  return static_cast<uint32_t>(now - relays[index].lastSwitchAt) >= kMinimumSwitchIntervalMs;
}

bool transactionIsNewer(uint32_t candidate, uint32_t previous) {
  return static_cast<int32_t>(candidate - previous) > 0;
}

void printStatus() {
  const uint32_t now = millis();
  Serial.printf("STATUS SESSION=%08lX UPTIME=%lu RESET=%u", static_cast<unsigned long>(sessionId),
                static_cast<unsigned long>(now / 1000U), ESP.getResetInfoPtr()->reason);
  for (uint8_t i = 0; i < netguard::kRelayCount; ++i) {
    const uint32_t remaining = relays[i].timerActive
      ? netguard::remainingSeconds(now, relays[i].deadline) : 0;
    Serial.printf(" R%u=%u T%u=%lu", i + 1, relays[i].on ? 1 : 0,
                  i + 1, static_cast<unsigned long>(remaining));
  }
  Serial.println();
}

void printError(netguard::ParseError error) {
  switch (error) {
    case netguard::ParseError::UnknownCommand: Serial.println(F("ERR COMMAND")); break;
    case netguard::ParseError::Channel: Serial.println(F("ERR CHANNEL")); break;
    case netguard::ParseError::Time: Serial.println(F("ERR TIME")); break;
    case netguard::ParseError::Transaction: Serial.println(F("ERR TRANSACTION")); break;
    default: Serial.println(F("ERR SYNTAX")); break;
  }
}

void executeLine(char* line) {
  const netguard::Command command = netguard::parseCommand(line);
  if (command.type == netguard::CommandType::Invalid) {
    printError(command.error);
    return;
  }
  switch (command.type) {
    case netguard::CommandType::Ping:
      Serial.println(F("PONG"));
      break;
    case netguard::CommandType::Version:
      Serial.printf("VERSION %s %s\r\n", kProduct, kVersion);
      break;
    case netguard::CommandType::Status:
      printStatus();
      break;
    case netguard::CommandType::On: {
      const uint8_t i = command.channel - 1;
      if (relays[i].timerActive) { Serial.println(F("ERR BUSY")); break; }
      setRelay(i, true);
      Serial.printf("OK ON %u\r\n", command.channel);
      break;
    }
    case netguard::CommandType::Off:
    case netguard::CommandType::Pulse: {
      const uint8_t i = command.channel - 1;
      if (relays[i].timerActive || !relayReady(i, millis())) { Serial.println(F("ERR BUSY")); break; }
      setRelay(i, false);
      relays[i].deadline = millis() + command.seconds * 1000U;
      relays[i].timerActive = true;
      relays[i].transactionId = 0;
      relays[i].transactionSession = 0;
      Serial.printf("OK %s %u %lu\r\n",
                    command.type == netguard::CommandType::Off ? "OFF" : "PULSE",
                    command.channel, static_cast<unsigned long>(command.seconds));
      break;
    }
    case netguard::CommandType::Reset: {
      const uint8_t i = command.channel - 1;
      if (command.sessionId == lastTransactionSession && command.transactionId == lastTransactionId) {
        if (relays[i].timerActive && command.channel == lastTransactionChannel &&
            command.seconds == lastTransactionSeconds)
          Serial.printf("OK RESET %lu %lu %u %lu\r\n", static_cast<unsigned long>(command.sessionId),
                        static_cast<unsigned long>(command.transactionId), command.channel,
                        static_cast<unsigned long>(command.seconds));
        else
          Serial.println(F("ERR TRANSACTION"));
        break;
      }
      if (command.sessionId == lastTransactionSession &&
          !transactionIsNewer(command.transactionId, lastTransactionId)) {
        Serial.println(F("ERR TRANSACTION"));
        break;
      }
      if (relays[i].timerActive || !relayReady(i, millis())) { Serial.println(F("ERR BUSY")); break; }
      setRelay(i, false);
      relays[i].deadline = millis() + command.seconds * 1000U;
      relays[i].timerActive = true;
      relays[i].transactionId = command.transactionId;
      relays[i].transactionSession = command.sessionId;
      lastTransactionSession = command.sessionId;
      lastTransactionId = command.transactionId;
      lastTransactionChannel = command.channel;
      lastTransactionSeconds = command.seconds;
      Serial.printf("OK RESET %lu %lu %u %lu\r\n", static_cast<unsigned long>(command.sessionId),
                    static_cast<unsigned long>(command.transactionId), command.channel,
                    static_cast<unsigned long>(command.seconds));
      break;
    }
    case netguard::CommandType::AllOn:
      for (uint8_t i = 0; i < netguard::kRelayCount; ++i) {
        if (relays[i].timerActive) { Serial.println(F("ERR BUSY")); return; }
      }
      for (uint8_t i = 0; i < netguard::kRelayCount; ++i) setRelay(i, true);
      Serial.println(F("OK ALLON"));
      break;
    case netguard::CommandType::Help:
      Serial.println(F("OK HELP PING VERSION STATUS RESET <SESSION> <TX> <1-4> <1-300> ON OFF PULSE ALLON HELP"));
      break;
    default:
      Serial.println(F("ERR COMMAND"));
      break;
  }
}

void finishLine() {
  if (discardingLongLine) {
    Serial.println(F("ERR LINE TOO LONG"));
  } else if (inputLength > 0) {
    inputBuffer[inputLength] = '\0';
    executeLine(inputBuffer);
  }
  inputLength = 0;
  discardingLongLine = false;
}

void serviceUart() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') {
      if (!(c == '\n' && previousWasCr)) finishLine();
      previousWasCr = (c == '\r');
      continue;
    }
    previousWasCr = false;
    if (discardingLongLine) continue;
    if (inputLength + 1 >= kInputCapacity) {
      inputLength = 0;
      discardingLongLine = true;
      continue;
    }
    inputBuffer[inputLength++] = c;
  }
}

void serviceTimers() {
  const uint32_t now = millis();
  for (uint8_t i = 0; i < netguard::kRelayCount; ++i) {
    if (relays[i].timerActive && netguard::deadlineReached(now, relays[i].deadline)) {
      setRelay(i, true);
      Serial.printf("EVENT CHANNEL_ON %lu %lu %u\r\n",
                    static_cast<unsigned long>(relays[i].transactionSession),
                    static_cast<unsigned long>(relays[i].transactionId), i + 1);
    }
  }
}

void serviceHeartbeat() {
  const uint32_t now = millis();
  if (!netguard::deadlineReached(now, nextHeartbeatAt)) return;
  nextHeartbeatAt = now + kHeartbeatIntervalMs;
  Serial.printf("HEARTBEAT %08lX %lu %u%u%u%u\r\n", static_cast<unsigned long>(sessionId),
                static_cast<unsigned long>(now / 1000U), relays[0].on, relays[1].on,
                relays[2].on, relays[3].on);
}

}  // namespace

void setup() {
  // Preload each output latch HIGH before enabling output mode to minimize OFF glitches.
  for (uint8_t i = 0; i < netguard::kRelayCount; ++i) {
    digitalWrite(kRelayPins[i], HIGH);
    pinMode(kRelayPins[i], OUTPUT);
    relays[i] = {true, false, 0, 0, 0, 0};
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();

  Serial.begin(9600, SERIAL_8N1);
  Serial.setDebugOutput(false);
  ESP.wdtEnable(8000);
  sessionId = ESP.getChipId() ^ micros() ^ ESP.getCycleCount();
  if (!sessionId) sessionId = 1;
  nextHeartbeatAt = millis() + kHeartbeatIntervalMs;
  Serial.printf("\r\nBOOT %s %s SESSION=%08lX RESET=%u\r\n", kProduct, kVersion,
                static_cast<unsigned long>(sessionId), ESP.getResetInfoPtr()->reason);
  printStatus();
}

void loop() {
  serviceUart();
  serviceTimers();
  serviceHeartbeat();
  ESP.wdtFeed();
  yield();
}
