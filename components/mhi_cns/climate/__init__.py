import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate

from .. import CONF_MHI_CNS_ID, MhiCnsComponent, mhi_cns_ns

MhiCnsClimate = mhi_cns_ns.class_(
    "MhiCnsClimate", climate.Climate, cg.Component, cg.Parented.template(MhiCnsComponent)
)

CONFIG_SCHEMA = climate.climate_schema(MhiCnsClimate).extend(
    {cv.GenerateID(CONF_MHI_CNS_ID): cv.use_id(MhiCnsComponent)}
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_MHI_CNS_ID])
    await cg.register_parented(var, parent)
