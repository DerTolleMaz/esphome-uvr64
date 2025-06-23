import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, binary_sensor
from esphome.const import CONF_ID, CONF_PIN

CODEOWNERS = ["@DerTolleMaz"]
DEPENDENCIES = []

uvr64_ns = cg.esphome_ns.namespace("uvr64_dlbus")
DLBusSensor = uvr64_ns.class_("DLBusSensor", cg.Component)

CONF_TEMP_SENSORS = "temp_sensors"
CONF_RELAY_SENSORS = "relay_sensors"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DLBusSensor),
    cv.Required(CONF_PIN): cv.pin,
    cv.Required(CONF_TEMP_SENSORS): cv.ensure_list(cv.use_id(sensor.Sensor)),
    cv.Required(CONF_RELAY_SENSORS): cv.ensure_list(cv.use_id(binary_sensor.BinarySensor)),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_PIN])
    await cg.register_component(var, config)

    for i, sens in enumerate(config[CONF_TEMP_SENSORS]):
        sens_var = await cg.get_variable(sens)
        cg.add(var.set_temp_sensor(i, sens_var))

    for i, sens in enumerate(config[CONF_RELAY_SENSORS]):
        sens_var = await cg.get_variable(sens)
        cg.add(var.set_relay_sensor(i, sens_var))
