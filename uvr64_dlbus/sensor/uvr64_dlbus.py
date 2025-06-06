
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_ID, CONF_UART_ID, UNIT_CELSIUS,
    DEVICE_CLASS_TEMPERATURE, ICON_THERMOMETER
)

CONF_TEMPERATURES = "temperatures"
CONF_RELAYS = "relays"

uvr64_ns = cg.esphome_ns.namespace("uvr64_dlbus")
UVR64DLBusSensor = uvr64_ns.class_("UVR64DLBusSensor", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(UVR64DLBusSensor),
    cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
    cv.Required(CONF_TEMPERATURES): cv.ensure_list(sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        icon=ICON_THERMOMETER,
        device_class=DEVICE_CLASS_TEMPERATURE)),
    cv.Required(CONF_RELAYS): cv.ensure_list(sensor.sensor_schema(
        accuracy_decimals=0))
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    uart_var = await cg.get_variable(config[CONF_UART_ID])
    cg.add(var.set_parent(uart_var))

    temps = []
    for s in config[CONF_TEMPERATURES]:
        sens = await sensor.new_sensor(s)
        temps.append(sens)
    cg.add(var.set_temps(temps))

    rels = []
    for s in config[CONF_RELAYS]:
        sens = await sensor.new_sensor(s)
        rels.append(sens)
    cg.add(var.set_relays(rels))
