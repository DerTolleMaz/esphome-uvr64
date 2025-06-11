import pytest
import importlib.util
import os

module_path = os.path.join(
    os.path.dirname(__file__),
    "..",
    "components",
    "uvr64_dlbus",
    "parse_dl_bus.py",
)
spec = importlib.util.spec_from_file_location("parse_dl_bus", module_path)
parse_mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(parse_mod)
parse_dl_bus = parse_mod.parse_dl_bus
DummySensor = parse_mod.DummySensor
# Example frame used for testing
SAMPLE_FRAME = [
    0xAA, 0x55, 0x00, 0x00, 0x00,
    0x00, 0x64,  # 10.0°C
    0x00, 0xC8,  # 20.0°C
    0x01, 0x2C,  # 30.0°C
    0x01, 0x90,  # 40.0°C
    0x01, 0xF4,  # 50.0°C
    0x02, 0x58,  # 60.0°C
    0x00, 0x00, 0x00,
    0x0D,        # relay status
] + [0x00]*10

# Add checksum
checksum = sum(SAMPLE_FRAME) & 0xFF
SAMPLE_FRAME.append(checksum)


def test_parse_dl_bus():
    temps = [DummySensor() for _ in range(6)]
    relays = [DummySensor() for _ in range(4)]

    parse_dl_bus(SAMPLE_FRAME, temps, relays)

    assert [t.state for t in temps] == [10.0, 20.0, 30.0, 40.0, 50.0, 60.0]
    assert [r.state for r in relays] == [1, 0, 1, 1]
