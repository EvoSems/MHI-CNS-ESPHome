import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from .. import CONF_MHI_CNS_ID, MhiCnsComponent

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_MHI_CNS_ID): cv.use_id(MhiCnsComponent),
    cv.Optional("error_status"): text_sensor.text_sensor_schema(icon="mdi:alert-outline"),
    cv.Optional("protection_status"): text_sensor.text_sensor_schema(icon="mdi:shield-alert-outline"),
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_MHI_CNS_ID])
    for key, setter in (("error_status", parent.set_error_text_sensor), ("protection_status", parent.set_protection_text_sensor)):
        if key in config:
            value = await text_sensor.new_text_sensor(config[key])
            cg.add(setter(value))
