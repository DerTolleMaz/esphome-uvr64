import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import pins
from esphome.const import CONF_ID

uvr64_dlbus_ns = cg.esphome_ns.namespace('uvr64_dlbus')
DLBusSensor = uvr64_dlbus_ns.class_('DLBusSensor', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DLBusSensor),
    cv.Required("pin"): pins.internal_gpio_input_pin_schema,
    cv.Required("temp_sensors"): cv.ensure_list(cv.use_id(cg.Sensor)),
    cv.Required("relay_sensors"): cv.ensure_list(cv.use_id(cg.BinarySensor)),
})
