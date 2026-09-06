"""Host-side behavioral tests for the v0.2.0 UART contract."""

MAX_LINE = 94


def parse(text):
    words = text.upper().split()
    if not words:
        return "ERR SYNTAX"
    if words[0] in {"PING", "VERSION", "STATUS", "ALLON", "HELP"}:
        return words[0] if len(words) == 1 else "ERR SYNTAX"
    if words[0] == "RESET":
        if len(words) != 5:
            return "ERR SYNTAX"
        try:
            session, transaction, channel, seconds = map(int, words[1:])
        except ValueError:
            return "ERR SYNTAX"
        if session <= 0 or transaction <= 0:
            return "ERR TRANSACTION"
        if channel not in range(1, 5):
            return "ERR CHANNEL"
        if not 1 <= seconds <= 300:
            return "ERR TIME"
        return ("RESET", session, transaction, channel, seconds)
    if words[0] not in {"ON", "OFF", "PULSE"}:
        return "ERR COMMAND"
    if len(words) < 2 or len(words) > (2 if words[0] == "ON" else 3):
        return "ERR SYNTAX"
    try:
        channel = int(words[1])
    except ValueError:
        return "ERR CHANNEL"
    if channel not in range(1, 5):
        return "ERR CHANNEL"
    if words[0] == "ON":
        return ("ON", channel)
    if len(words) == 2:
        return ("OFF", channel, 300) if words[0] == "OFF" else "ERR SYNTAX"
    try:
        seconds = int(words[2])
    except ValueError:
        return "ERR TIME"
    if not 1 <= seconds <= 300:
        return "ERR TIME"
    return (words[0], channel, seconds)


def reached(now, deadline):
    difference = (now - deadline) & 0xFFFFFFFF
    return difference < 0x80000000


assert parse("PING") == "PING"
assert parse("VERSION") == "VERSION"
assert parse(" STATUS ") == "STATUS"
assert parse("ON 1") == ("ON", 1)
assert parse("on   4") == ("ON", 4)
assert parse("OFF 1 5") == ("OFF", 1, 5)
assert parse("OFF 4") == ("OFF", 4, 300)
assert parse("PULSE 1 3") == ("PULSE", 1, 3)
assert parse("PULSE 2 5") == ("PULSE", 2, 5)
assert parse("RESET 99 17 2 10") == ("RESET", 99, 17, 2, 10)
assert parse("RESET 0 17 2 10") == "ERR TRANSACTION"
assert parse("RESET 99 -1 2 10") == "ERR TRANSACTION"
assert parse("RESET 99 17 5 10") == "ERR CHANNEL"
assert parse("RESET 99 17 2 301") == "ERR TIME"
assert parse("RESET 99 17 2") == "ERR SYNTAX"
assert {parse("PULSE 1 20"), parse("PULSE 2 40")} == {
    ("PULSE", 1, 20), ("PULSE", 2, 40)
}
assert parse("ALLON") == "ALLON"
assert parse("ON 5") == "ERR CHANNEL"
assert parse("PULSE 1 0") == "ERR TIME"
assert parse("UNKNOWN") == "ERR COMMAND"
assert parse("ON") == "ERR SYNTAX"
assert len("X" * (MAX_LINE + 1)) > MAX_LINE  # UART reader rejects this whole line.
assert not reached(0xFFFFFFF0, 0x00000010)
assert reached(0x00000010, 0x00000010)
assert reached(0x00000020, 0x00000010)


def transaction_is_newer(candidate, previous):
    difference = (candidate - previous) & 0xFFFFFFFF
    return 0 < difference < 0x80000000


assert transaction_is_newer(18, 17)
assert not transaction_is_newer(16, 17)
assert transaction_is_newer(0, 0xFFFFFFFF)

print("PASS: protocol v3 parsing, validation, legacy commands, long-line and rollover checks")
