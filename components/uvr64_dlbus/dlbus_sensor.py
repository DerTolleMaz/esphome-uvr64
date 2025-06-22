import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import sensor, binary_sensor

uvr64_dlbus_ns = cg.esphome_ns.namespace('uvr64_dlbus')
DLBusSensor = uvr64_dlbus_ns.class_('DLBusSensor', cg.Component)

CONF_ID = "uvr"
CONF_PIN = "pin"
CONF_TEMP_SENSORS = "temp_sensors"
CONF_RELAY_SENSORS = "relay_sensors"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DLBusSensor),
    cv.Required(CONF_PIN): pins.gpio_input_pin_schema,
    cv.Required(CONF_TEMP_SENSORS): cv.ensure_list(cv.use_id(sensor.Sensor)),
    cv.Required(CONF_RELAY_SENSORS): cv.ensure_list(cv.use_id(binary_sensor.BinarySensor)),
})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    for i, s in enumerate(config[CONF_TEMP_SENSORS]):
        sens = await cg.get_variable(s)
        cg.add(var.set_temp_sensor(i, sens))

    for i, s in enumerate(config[CONF_RELAY_SENSORS]):
        relay = await cg.get_variable(s)
        cg.add(var.set_relay_sensor(i, relay))
