import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, binary_sensor
from esphome.const import CONF_PIN

CONF_TEMP_SENSORS = "temp_sensors"
CONF_RELAY_SENSORS = "relay_sensors"

uvr64_dlbus_ns = cg.esphome_ns.namespace("uvr64_dlbus")
DLBusSensor = uvr64_dlbus_ns.class_(
    "DLBusSensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(DLBusSensor)
    .extend(
        {
            cv.Required(CONF_PIN): pins.internal_gpio_input_pin_number,
            cv.Optional(CONF_TEMP_SENSORS, default=[]): cv.All(
                cv.ensure_list(sensor.sensor_schema()), cv.Length(max=6)
            ),
            cv.Optional(CONF_RELAY_SENSORS, default=[]): cv.All(
                cv.ensure_list(binary_sensor.binary_sensor_schema()),
                cv.Length(max=4),
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config, config[CONF_PIN])
    await cg.register_component(var, config)

    for i, conf in enumerate(config.get(CONF_TEMP_SENSORS, [])):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_temp_sensor(i, sens))

    for i, conf in enumerate(config.get(CONF_RELAY_SENSORS, [])):
        bs = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_relay_sensor(i, bs))
