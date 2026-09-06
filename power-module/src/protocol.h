#pragma once

#include <stddef.h>
#include <stdint.h>

namespace netguard {

constexpr uint8_t kRelayCount = 4;
constexpr uint32_t kMinSeconds = 1;
constexpr uint32_t kMaxSeconds = 300;
constexpr uint32_t kDefaultOffSeconds = 300;

enum class CommandType : uint8_t { Ping, Version, Status, On, Off, Pulse, Reset, AllOn, Help, Invalid };
enum class ParseError : uint8_t { None, UnknownCommand, Syntax, Channel, Time, Transaction };

struct Command {
  CommandType type;
  uint8_t channel;
  uint32_t seconds;
  uint32_t sessionId;
  uint32_t transactionId;
  ParseError error;
};

Command parseCommand(char* line);
bool deadlineReached(uint32_t now, uint32_t deadline);
uint32_t remainingSeconds(uint32_t now, uint32_t deadline);

}  // namespace netguard
