#include "protocol.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace netguard {
namespace {

constexpr size_t kMaxTokens = 5;

bool equals(const char* a, const char* b) { return strcmp(a, b) == 0; }

bool parseUnsigned(const char* text, uint32_t& value) {
  if (text == nullptr || *text == '\0' || !isdigit(static_cast<unsigned char>(*text))) return false;
  char* end = nullptr;
  unsigned long parsed = strtoul(text, &end, 10);
  if (*end != '\0' || parsed > 0xFFFFFFFFUL) return false;
  value = static_cast<uint32_t>(parsed);
  return true;
}

Command invalid(ParseError error) { return {CommandType::Invalid, 0, 0, 0, 0, error}; }

}  // namespace

Command parseCommand(char* line) {
  char* tokens[kMaxTokens] = {};
  size_t count = 0;
  char* p = line;
  while (*p != '\0') {
    while (isspace(static_cast<unsigned char>(*p))) ++p;
    if (*p == '\0') break;
    if (count == kMaxTokens) return invalid(ParseError::Syntax);
    tokens[count++] = p;
    while (*p != '\0' && !isspace(static_cast<unsigned char>(*p))) {
      *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
      ++p;
    }
    if (*p != '\0') *p++ = '\0';
  }
  if (count == 0) return invalid(ParseError::Syntax);

  struct Simple { const char* name; CommandType type; };
  const Simple simple[] = {{"PING", CommandType::Ping}, {"VERSION", CommandType::Version},
                           {"STATUS", CommandType::Status}, {"ALLON", CommandType::AllOn},
                           {"HELP", CommandType::Help}};
  for (const auto& item : simple) {
    if (equals(tokens[0], item.name))
      return count == 1 ? Command{item.type, 0, 0, 0, 0, ParseError::None} : invalid(ParseError::Syntax);
  }

  if (equals(tokens[0], "RESET")) {
    if (count != 5) return invalid(ParseError::Syntax);
    uint32_t session = 0, transaction = 0, channel = 0, seconds = 0;
    if (!parseUnsigned(tokens[1], session) || session == 0 ||
        !parseUnsigned(tokens[2], transaction) || transaction == 0)
      return invalid(ParseError::Transaction);
    if (!parseUnsigned(tokens[3], channel) || channel < 1 || channel > kRelayCount)
      return invalid(ParseError::Channel);
    if (!parseUnsigned(tokens[4], seconds) || seconds < kMinSeconds || seconds > kMaxSeconds)
      return invalid(ParseError::Time);
    return {CommandType::Reset, static_cast<uint8_t>(channel), seconds, session, transaction,
            ParseError::None};
  }

  CommandType type = CommandType::Invalid;
  if (equals(tokens[0], "ON")) type = CommandType::On;
  else if (equals(tokens[0], "OFF")) type = CommandType::Off;
  else if (equals(tokens[0], "PULSE")) type = CommandType::Pulse;
  else return invalid(ParseError::UnknownCommand);

  const size_t minCount = (type == CommandType::On) ? 2 : 2;
  const size_t maxCount = (type == CommandType::On) ? 2 : 3;
  if (count < minCount || count > maxCount) return invalid(ParseError::Syntax);

  uint32_t channel = 0;
  if (!parseUnsigned(tokens[1], channel) || channel < 1 || channel > kRelayCount)
    return invalid(ParseError::Channel);

  uint32_t seconds = 0;
  if (type != CommandType::On) {
    if (count == 2) {
      if (type == CommandType::Pulse) return invalid(ParseError::Syntax);
      seconds = kDefaultOffSeconds;
    } else if (!parseUnsigned(tokens[2], seconds) || seconds < kMinSeconds || seconds > kMaxSeconds) {
      return invalid(ParseError::Time);
    }
  }
  return {type, static_cast<uint8_t>(channel), seconds, 0, 0, ParseError::None};
}

bool deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

uint32_t remainingSeconds(uint32_t now, uint32_t deadline) {
  if (deadlineReached(now, deadline)) return 0;
  return (deadline - now + 999U) / 1000U;
}

}  // namespace netguard
