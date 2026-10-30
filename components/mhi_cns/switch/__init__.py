import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import DEVICE_CLASS_SWITCH

from .. import CONF_MHI_CNS_ID, MhiCnsComponent, mhi_cns_ns

CONF_POWER = "power"
MhiCnsPowerSwitch = mhi_cns_ns.class_(
    "MhiCnsPowerSwitch", switch.Switch, cg.Component, cg.Parented.template(MhiCnsComponent)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MHI_CNS_ID): cv.use_id(MhiCnsComponent),
        cv.Required(CONF_POWER): switch.switch_schema(
            MhiCnsPowerSwitch, device_class=DEVICE_CLASS_SWITCH, icon="mdi:power"
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MHI_CNS_ID])
    var = await switch.new_switch(config[CONF_POWER])
    await cg.register_component(var, config[CONF_POWER])
    await cg.register_parented(var, parent)
