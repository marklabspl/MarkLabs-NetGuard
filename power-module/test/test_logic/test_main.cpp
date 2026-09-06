#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "protocol.h"

using namespace netguard;

Command parse(const char* input) {
  char line[128];
  strncpy(line, input, sizeof(line));
  line[sizeof(line) - 1] = '\0';
  return parseCommand(line);
}

int main() {
  assert(parse("PING").type == CommandType::Ping);
  assert(parse("version").type == CommandType::Version);
  assert(parse("  STATUS  ").type == CommandType::Status);
  assert(parse("ON 1").channel == 1);
  assert(parse("on   4").channel == 4);
  assert(parse("OFF 1 5").seconds == 5);
  assert(parse("OFF 4").seconds == 300);
  assert(parse("PULSE 1 3").seconds == 3);
  assert(parse("PULSE 2 5").seconds == 5);
  Command reset = parse("RESET 99 17 2 10");
  assert(reset.type == CommandType::Reset && reset.sessionId == 99 && reset.transactionId == 17);
  assert(reset.channel == 2 && reset.seconds == 10);
  assert(parse("RESET 0 17 2 10").error == ParseError::Transaction);
  assert(parse("RESET 99 -1 2 10").error == ParseError::Transaction);
  assert(parse("RESET 99 17 5 10").error == ParseError::Channel);
  assert(parse("RESET 99 17 2 301").error == ParseError::Time);
  assert(parse("RESET 99 17 2").error == ParseError::Syntax);
  Command pulse1 = parse("PULSE 1 20"), pulse2 = parse("PULSE 2 40");
  assert(pulse1.channel != pulse2.channel && pulse1.seconds == 20 && pulse2.seconds == 40);
  assert(parse("ALLON").type == CommandType::AllOn);
  assert(parse("ON 5").error == ParseError::Channel);
  assert(parse("PULSE 1 0").error == ParseError::Time);
  assert(parse("BOGUS").error == ParseError::UnknownCommand);
  assert(parse("ON").error == ParseError::Syntax);
  assert(parse("PING extra").error == ParseError::Syntax);
  assert(parse("PULSE 1 301").error == ParseError::Time);

  // Wrap-safe deadline checks: deadline is 16 ms after uint32_t rollover.
  const uint32_t beforeWrap = 0xFFFFFFF0U;
  const uint32_t afterWrapDeadline = 0x00000010U;
  assert(!deadlineReached(beforeWrap, afterWrapDeadline));
  assert(deadlineReached(0x00000010U, afterWrapDeadline));
  assert(deadlineReached(0x00000020U, afterWrapDeadline));
  assert(static_cast<int32_t>(18U - 17U) > 0);
  assert(static_cast<int32_t>(16U - 17U) < 0);
  assert(static_cast<int32_t>(0U - 0xffffffffU) > 0);

  // Input buffer overflow behavior is integration-tested by the fixed-capacity UART reader;
  // parser independently rejects excess tokens without executing a partial command.
  assert(parse("ON 1 EXTRA EXTRA").error == ParseError::Syntax);
  puts("All NetGuard protocol logic tests passed.");
  return 0;
}
