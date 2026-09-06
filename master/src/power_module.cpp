#include "power_module.h"

#include <cstdlib>
#include <cstring>
#include <esp_system.h>

#include "config.h"

PowerModule::PowerModule(HardwareSerial& serial) : serial_(serial) {}

void PowerModule::begin(int rxPin, int txPin, uint32_t baud) {
  serial_.begin(baud, SERIAL_8N1, rxPin, txPin);
  masterSessionId_ = esp_random();
  if (!masterSessionId_) masterSessionId_ = 1;
  nextTransactionId_ = esp_random();
  if (!nextTransactionId_) nextTransactionId_ = 1;
  nextPingAt_ = millis() + 1000U;
}

bool PowerModule::reached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

uint32_t PowerModule::lastResponseAge(uint32_t now) const {
  return lastResponseAt_ == 0 ? UINT32_MAX : now - lastResponseAt_;
}

bool PowerModule::requestPulse(uint8_t channel,uint16_t seconds,uint32_t now){
  if(!online_||commandPending_||channel<1||channel>4||seconds<1||seconds>300)return false;
  pendingTransactionId_=nextTransactionId_++;
  if(!nextTransactionId_)nextTransactionId_=1;
  serial_.printf("RESET %lu %lu %u %u\r\n",static_cast<unsigned long>(masterSessionId_),static_cast<unsigned long>(pendingTransactionId_),channel,seconds);commandPending_=true;resetAccepted_=false;pendingChannel_=channel;pendingSeconds_=seconds;commandSentAt_=now;completionDeadline_=now+static_cast<uint32_t>(seconds)*1000U+config::kResetCompletionMarginMs;return true;
}

void PowerModule::sendPing(uint32_t now) {
  serial_.print("PING\r\n");
  awaitingPong_ = true;
  pingSentAt_ = now;
}

void PowerModule::handleLine(uint32_t now) {
  if(strncmp(line_,"OK RESET ",9)==0){char expected[64];snprintf(expected,sizeof(expected),"OK RESET %lu %lu %u %u",static_cast<unsigned long>(masterSessionId_),static_cast<unsigned long>(pendingTransactionId_),pendingChannel_,pendingSeconds_);if(commandPending_&&!strcmp(line_,expected)){resetAccepted_=true;lastResponseAt_=now;online_=true;nextPingAt_=now+config::kHeartbeatIntervalMs;}else{++invalidLines_;}return;}
  if(strncmp(line_,"ERR ",4)==0){lastResponseAt_=now;online_=true;if(commandPending_){commandPending_=false;resetAccepted_=false;pendingChannel_=0;pendingSeconds_=0;pendingTransactionId_=0;++commandFailures_;}return;}
  if (strcmp(line_, "PONG") == 0) {
    awaitingPong_ = false;
    online_ = true;
    lastResponseAt_ = now;
    ++successfulHeartbeats_;
    if (consecutiveHeartbeats_ < UINT8_MAX) ++consecutiveHeartbeats_;
    nextPingAt_ = now + config::kHeartbeatIntervalMs;
    return;
  }
  if (strncmp(line_, "STATUS ", 7) == 0) {
    strncpy(lastStatus_, line_, sizeof(lastStatus_) - 1);
    lastStatus_[sizeof(lastStatus_) - 1] = '\0';
    lastResponseAt_ = now;
    online_ = true;
    return;
  }
  if (strncmp(line_, "BOOT NETGUARD-POWER ", 20) == 0) {
    const char* session = strstr(line_, "SESSION=");
    if (session) {
      const uint32_t parsed = strtoul(session + 8, nullptr, 16);
      if (sessionId_ && parsed && parsed != sessionId_) ++moduleRestarts_;
      if (parsed) sessionId_ = parsed;
    }
    if (commandPending_) { commandPending_=false; resetAccepted_=false; pendingChannel_=0; pendingSeconds_=0; pendingTransactionId_=0; ++commandFailures_; }
    consecutiveHeartbeats_=0; lastResponseAt_=now; online_=true;
    return;
  }
  if (strncmp(line_, "HEARTBEAT ", 10) == 0) {
    unsigned long session=0, uptime=0; char states[5]={};
    int consumed=0;
    if(sscanf(line_,"HEARTBEAT %lx %lu %4[01]%n",&session,&uptime,states,&consumed)==3 && line_[consumed]=='\0'){
      const uint32_t parsed=static_cast<uint32_t>(session);
      if(sessionId_&&parsed!=sessionId_){++moduleRestarts_;if(commandPending_){commandPending_=false;resetAccepted_=false;pendingChannel_=0;pendingSeconds_=0;pendingTransactionId_=0;++commandFailures_;}}
      sessionId_=parsed;strlcpy(relayStates_,states,sizeof(relayStates_));
      lastResponseAt_=now;online_=true;awaitingPong_=false;
      if(consecutiveHeartbeats_<UINT8_MAX)++consecutiveHeartbeats_;
      ++successfulHeartbeats_;nextPingAt_=now+config::kHeartbeatIntervalMs;return;
    }
    ++invalidLines_;return;
  }
  if (strncmp(line_, "EVENT ", 6) == 0) {
    unsigned long masterSession=0,transaction=0;unsigned channel=0;int consumed=0;
    if(sscanf(line_,"EVENT CHANNEL_ON %lu %lu %u%n",&masterSession,&transaction,&channel,&consumed)==3&&line_[consumed]=='\0'&&commandPending_&&resetAccepted_&&masterSession==masterSessionId_&&transaction==pendingTransactionId_&&channel==pendingChannel_){
      commandPending_=false;resetAccepted_=false;pendingChannel_=0;pendingSeconds_=0;pendingTransactionId_=0;lastResponseAt_=now;online_=true;++acceptedPulses_;return;
    }
    ++invalidLines_;return;
  }
  ++invalidLines_;
}

void PowerModule::finishLine(uint32_t now) {
  if (discarding_) {
    ++invalidLines_;
  } else if (lineLength_ > 0) {
    line_[lineLength_] = '\0';
    handleLine(now);
  }
  lineLength_ = 0;
  discarding_ = false;
}

void PowerModule::serviceRx(uint32_t now) {
  while (serial_.available() > 0) {
    const char c = static_cast<char>(serial_.read());
    if (c == '\r' || c == '\n') {
      if (!(c == '\n' && previousWasCr_)) finishLine(now);
      previousWasCr_ = (c == '\r');
      continue;
    }
    previousWasCr_ = false;
    if (discarding_) continue;
    if (lineLength_ + 1 >= sizeof(line_)) {
      lineLength_ = 0;
      discarding_ = true;
    } else {
      line_[lineLength_++] = c;
    }
  }
}

void PowerModule::update(uint32_t now) {
  serviceRx(now);
  if(commandPending_&&((!resetAccepted_&&reached(now,commandSentAt_+config::kHeartbeatTimeoutMs))||(resetAccepted_&&reached(now,completionDeadline_)))){commandPending_=false;resetAccepted_=false;pendingChannel_=0;pendingSeconds_=0;pendingTransactionId_=0;++commandFailures_;online_=false;consecutiveHeartbeats_=0;}
  if (awaitingPong_ && reached(now, pingSentAt_ + config::kHeartbeatTimeoutMs)) {
    awaitingPong_ = false;
    online_ = false;
    consecutiveHeartbeats_ = 0;
    ++timeouts_;
    nextPingAt_ = now + config::kHeartbeatIntervalMs;
  }
  if (!awaitingPong_ && (!commandPending_ || resetAccepted_) && reached(now, nextPingAt_)) sendPing(now);
}
