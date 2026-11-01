import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.core import CORE
from esphome.const import DEVICE_CLASS_CONNECTIVITY, DEVICE_CLASS_RUNNING, ENTITY_CATEGORY_DIAGNOSTIC
from .. import CONF_MHI_CNS_ID, MhiCnsComponent

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_MHI_CNS_ID): cv.use_id(MhiCnsComponent),
    cv.Optional("healthy"): binary_sensor.binary_sensor_schema(device_class=DEVICE_CLASS_CONNECTIVITY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    cv.Optional("defrost"): binary_sensor.binary_sensor_schema(icon="mdi:snowflake-melt"),
    cv.Optional("compressor_running"): binary_sensor.binary_sensor_schema(device_class=DEVICE_CLASS_RUNNING),
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_MHI_CNS_ID])
    show_sensors = CORE.config.get("mhi_cns", {}).get("show_sensors", True)
    for key, setter in (("healthy", parent.set_healthy_sensor), ("defrost", parent.set_defrost_sensor), ("compressor_running", parent.set_compressor_running_sensor)):
        if key in config:
            entity_config = config[key] if show_sensors else {**config[key], "internal": True}
            value = await binary_sensor.new_binary_sensor(entity_config)
            cg.add(setter(value))
