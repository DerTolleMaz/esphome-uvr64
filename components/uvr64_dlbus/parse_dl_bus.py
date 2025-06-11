"""Helper functions for testing the UVR64 parser."""

from typing import List

class DummySensor:
    """Simple sensor placeholder used for unit tests."""

    def __init__(self) -> None:
        self.state = None

    def publish_state(self, value) -> None:
        self.state = value


def parse_dl_bus(data: List[int], temp_sensors: List[DummySensor], relay_sensors: List[DummySensor]):
    """Parse a raw DL-Bus frame and update sensors.

    The function mimics the logic implemented in the C++ component.
    """
    for i, sens in enumerate(temp_sensors[:6]):
        raw = (data[5 + i * 2] << 8) | data[6 + i * 2]
        sens.publish_state(raw / 10.0)

    if len(data) > 20:
        status = data[20]
        for i, sens in enumerate(relay_sensors[:8]):
            sens.publish_state((status >> i) & 0x01)
