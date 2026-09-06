#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>

class PowerModule {
 public:
  explicit PowerModule(HardwareSerial& serial);
  void begin(int rxPin, int txPin, uint32_t baud);
  void update(uint32_t now);

  bool online() const { return online_; }
  uint32_t lastResponseAge(uint32_t now) const;
  uint32_t successfulHeartbeats() const { return successfulHeartbeats_; }
  uint8_t consecutiveHeartbeats() const { return consecutiveHeartbeats_; }
  uint32_t timeouts() const { return timeouts_; }
  uint32_t invalidLines() const { return invalidLines_; }
  const char* lastStatus() const { return lastStatus_; }
  bool requestPulse(uint8_t channel, uint16_t seconds, uint32_t now);
  bool commandPending() const { return commandPending_; }
  bool resetAccepted() const { return resetAccepted_; }
  uint32_t acceptedPulses() const { return acceptedPulses_; }
  uint32_t commandFailures() const { return commandFailures_; }
  uint32_t sessionId() const { return sessionId_; }
  uint32_t moduleRestarts() const { return moduleRestarts_; }
  const char* relayStates() const { return relayStates_; }

 private:
  static constexpr size_t kLineCapacity = 128;
  static constexpr size_t kStatusCapacity = 128;

  HardwareSerial& serial_;
  char line_[kLineCapacity] = {};
  char lastStatus_[kStatusCapacity] = "UNKNOWN";
  size_t lineLength_ = 0;
  bool discarding_ = false;
  bool previousWasCr_ = false;
  bool awaitingPong_ = false;
  bool online_ = false;
  uint32_t pingSentAt_ = 0;
  uint32_t nextPingAt_ = 0;
  uint32_t lastResponseAt_ = 0;
  uint32_t successfulHeartbeats_ = 0;
  uint8_t consecutiveHeartbeats_ = 0;
  uint32_t timeouts_ = 0;
  uint32_t invalidLines_ = 0;
  bool commandPending_ = false;
  bool resetAccepted_ = false;
  uint32_t commandSentAt_ = 0;
  uint32_t acceptedPulses_ = 0;
  uint32_t commandFailures_ = 0;
  uint8_t pendingChannel_ = 0;
  uint16_t pendingSeconds_ = 0;
  uint32_t pendingTransactionId_ = 0;
  uint32_t nextTransactionId_ = 1;
  uint32_t masterSessionId_ = 1;
  uint32_t completionDeadline_ = 0;
  uint32_t sessionId_ = 0;
  uint32_t moduleRestarts_ = 0;
  char relayStates_[5] = "????";

  void serviceRx(uint32_t now);
  void finishLine(uint32_t now);
  void handleLine(uint32_t now);
  void sendPing(uint32_t now);
  static bool reached(uint32_t now, uint32_t deadline);
};
