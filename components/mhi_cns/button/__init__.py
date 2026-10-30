import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from .. import CONF_MHI_CNS_ID, MhiCnsComponent, mhi_cns_ns

CONF_SELF_CLEAN = "self_clean"
CONF_DURATION = "duration"
MhiCnsSelfCleanButton = mhi_cns_ns.class_(
    "MhiCnsSelfCleanButton", button.Button, cg.Component, cg.Parented.template(MhiCnsComponent)
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_MHI_CNS_ID): cv.use_id(MhiCnsComponent),
    cv.Optional(CONF_SELF_CLEAN): button.button_schema(MhiCnsSelfCleanButton, icon="mdi:air-filter").extend({
        cv.Optional(CONF_DURATION, default="120min"): cv.positive_time_period_milliseconds,
    }),
})

async def to_code(config):
    if value := config.get(CONF_SELF_CLEAN):
        parent = await cg.get_variable(config[CONF_MHI_CNS_ID])
        var = await button.new_button(value)
        await cg.register_component(var, value)
        await cg.register_parented(var, parent)
        cg.add(var.set_duration(value[CONF_DURATION].total_milliseconds))
