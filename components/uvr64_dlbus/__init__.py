import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN

from esphome.components import sensor, binary_sensor

uvr64_dlbus_ns = cg.esphome_ns.namespace('uvr64_dlbus')
DLBusSensor = uvr64_dlbus_ns.class_('DLBusSensor', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DLBusSensor),
    cv.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
    cv.Required("temp_sensors"): cv.ensure_list(cv.use_id(sensor.Sensor)),
    cv.Required("relay_sensors"): cv.ensure_list(cv.use_id(binary_sensor.BinarySensor)),
})


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    for i, sensor_id in enumerate(config["temp_sensors"]):
        sens = await cg.get_variable(sensor_id)
        cg.add(var.set_temp_sensor(i, sens))

    for i, relay_id in enumerate(config["relay_sensors"]):
        relay = await cg.get_variable(relay_id)
        cg.add(var.set_relay_sensor(i, relay))
