import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, binary_sensor
from esphome.const import CONF_ID, CONF_PIN

uvr64_dlbus_ns = cg.esphome_ns.namespace('uvr64_dlbus')
DLBusSensor = uvr64_dlbus_ns.class_('DLBusSensor', cg.Component)

CONF_TEMP_SENSORS = "temp_sensors"
CONF_RELAY_SENSORS = "relay_sensors"

CONF_DEBUG = "debug"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DLBusSensor),
    cv.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
    cv.Required(CONF_TEMP_SENSORS): cv.All(
        cv.ensure_list(cv.use_id(sensor.Sensor)),
        cv.Length(min=6, max=6, msg="Exactly 6 temp_sensors must be specified"),
    ),
    cv.Required(CONF_RELAY_SENSORS): cv.All(
        cv.ensure_list(cv.use_id(binary_sensor.BinarySensor)),
        cv.Length(min=4, max=4, msg="Exactly 4 relay_sensors must be specified"),
    ),
    cv.Optional(CONF_DEBUG, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], pin)
    await cg.register_component(var, config)

    cg.add(var.set_pin(pin))

    for i, sens in enumerate(config[CONF_TEMP_SENSORS]):
        sens_var = await cg.get_variable(sens)
        cg.add(var.set_temp_sensor(i, sens_var))

    for i, bsens in enumerate(config[CONF_RELAY_SENSORS]):
        bsens_var = await cg.get_variable(bsens)
        cg.add(var.set_relay_sensor(i, bsens_var))

    cg.add(var.set_debug(config[CONF_DEBUG]))
