from .sensor.uvr64_dlbus import (
    CONF_TEMPERATURES,
    CONF_RELAYS,
    CONF_DECODE_XOR,
    UVR64DLBusSensor,
    CONFIG_SCHEMA,
    to_code,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor"]
