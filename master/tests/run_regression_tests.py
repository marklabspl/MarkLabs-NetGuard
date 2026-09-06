"""Source-level regression checks for production safety invariants."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
power = (ROOT / "src" / "power_module.cpp").read_text(encoding="utf-8")
logic = (ROOT / "src" / "logic_engine.cpp").read_text(encoding="utf-8")
mqtt = (ROOT / "src" / "home_assistant_mqtt.cpp").read_text(encoding="utf-8")
config = (ROOT / "src" / "device_config.cpp").read_text(encoding="utf-8")
server = (ROOT / "src" / "status_server.cpp").read_text(encoding="utf-8")

assert 'EVENT CHANNEL_ON %lu %lu %u%n' in power
assert 'RESET %lu %lu %u %u' in power
assert 'resetAccepted_=true' in power
assert '++acceptedPulses_' in power
assert power.index('EVENT CHANNEL_ON %lu %lu %u%n') < power.rindex('++acceptedPulses_')
assert 'saveRecoveryCounters();' in logic
assert 'return fail("HOURLY_LIMIT")' in logic
assert 'connect(address, port, target.timeoutMs)' in logic
assert 'settings.discoveryPrefix) + "/status"' in mqtt
assert 'discoveryPending_ = discoveryPublishFailed_' in mqtt
assert 'validStaticNetwork' in config
assert 'Origin required' in server
assert 'if(recoveryCountersChanged)saveRecoveryCounters();' in logic

print("PASS: Master production-safety regression invariants")
