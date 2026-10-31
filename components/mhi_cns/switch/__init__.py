import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import DEVICE_CLASS_SWITCH

from .. import CONF_MHI_CNS_ID, MhiCnsComponent, mhi_cns_ns

CONF_POWER = "power"
CONF_AUTO_3D = "auto_3d"
MhiCnsPowerSwitch = mhi_cns_ns.class_(
    "MhiCnsPowerSwitch", switch.Switch, cg.Component, cg.Parented.template(MhiCnsComponent)
)
MhiCnsAuto3dSwitch = mhi_cns_ns.class_(
    "MhiCnsAuto3dSwitch", switch.Switch, cg.Component, cg.Parented.template(MhiCnsComponent)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MHI_CNS_ID): cv.use_id(MhiCnsComponent),
        cv.Required(CONF_POWER): switch.switch_schema(
            MhiCnsPowerSwitch, device_class=DEVICE_CLASS_SWITCH, icon="mdi:power"
        ),
        cv.Optional(CONF_AUTO_3D): switch.switch_schema(
            MhiCnsAuto3dSwitch, device_class=DEVICE_CLASS_SWITCH, icon="mdi:video-3d"
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MHI_CNS_ID])
    var = await switch.new_switch(config[CONF_POWER])
    await cg.register_component(var, config[CONF_POWER])
    await cg.register_parented(var, parent)
    if auto_3d := config.get(CONF_AUTO_3D):
        var = await switch.new_switch(auto_3d)
        await cg.register_component(var, auto_3d)
        await cg.register_parented(var, parent)
