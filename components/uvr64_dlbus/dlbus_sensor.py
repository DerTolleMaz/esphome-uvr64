import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, binary_sensor

uvr64_dlbus_ns = cg.esphome_ns.namespace('uvr64_dlbus')
DLBusSensor = uvr64_dlbus_ns.class_('DLBusSensor', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DLBusSensor),
    cv.Required("pin"): pins.gpio_input_pin_schema,
    cv.Required("temp_sensors"): cv.ensure_list(cv.use_id(sensor.Sensor)),
    cv.Required("relay_sensors"): cv.ensure_list(cv.use_id(binary_sensor.BinarySensor)),
})

async def to_code(config):
    var = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config["pin"])
    cg.add(var.set_pin(pin))

    for i, sens in enumerate(config["temp_sensors"]):
        s = await cg.get_variable(sens)
        cg.add(var.set_temp_sensor(i, s))

    for i, relay in enumerate(config["relay_sensors"]):
        r = await cg.get_variable(relay)
        cg.add(var.set_relay_sensor(i, r))
