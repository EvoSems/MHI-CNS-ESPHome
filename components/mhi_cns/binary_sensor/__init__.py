import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_CONNECTIVITY, DEVICE_CLASS_RUNNING, ENTITY_CATEGORY_DIAGNOSTIC
from .. import CONF_MHI_CNS_ID, MhiCnsComponent

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_MHI_CNS_ID): cv.use_id(MhiCnsComponent),
    cv.Optional("healthy"): binary_sensor.binary_sensor_schema(device_class=DEVICE_CLASS_CONNECTIVITY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    cv.Optional("defrost"): binary_sensor.binary_sensor_schema(icon="mdi:snowflake-melt"),
    cv.Optional("compressor_running"): binary_sensor.binary_sensor_schema(device_class=DEVICE_CLASS_RUNNING),
    cv.Optional("vacant"): binary_sensor.binary_sensor_schema(icon="mdi:home-export-outline"),
    cv.Optional("self_clean_operation"): binary_sensor.binary_sensor_schema(icon="mdi:air-filter"),
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_MHI_CNS_ID])
    for key, setter in (("healthy", parent.set_healthy_sensor), ("defrost", parent.set_defrost_sensor), ("compressor_running", parent.set_compressor_running_sensor), ("vacant", parent.set_vacant_sensor), ("self_clean_operation", parent.set_self_clean_operation_sensor)):
        if key in config:
            value = await binary_sensor.new_binary_sensor(config[key])
            cg.add(setter(value))
