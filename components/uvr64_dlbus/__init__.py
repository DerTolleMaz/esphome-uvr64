from esphome.codegen import register_component
import esphome.config_validation as cv

uvr64_dlbus_ns = cg.esphome_ns.namespace("uvr64_dlbus")
DLBusSensor = uvr64_dlbus_ns.class_("DLBusSensor", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DLBusSensor),
        cv.Required("pin"): cv.pin,
        cv.Optional("temp_sensors"): cv.ensure_list(cv.use_id(sensor.Sensor)),
        cv.Optional("relay_sensors"): cv.ensure_list(cv.use_id(binary_sensor.BinarySensor)),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config["pin"])
    await cg.register_component(var, config)

    for i, sensor_id in enumerate(config.get("temp_sensors", [])):
        sens = await cg.get_variable(sensor_id)
        cg.add(var.set_temp_sensor(i, sens))

    for i, relay_id in enumerate(config.get("relay_sensors", [])):
        rel = await cg.get_variable(relay_id)
        cg.add(var.set_relay_sensor(i, rel))
