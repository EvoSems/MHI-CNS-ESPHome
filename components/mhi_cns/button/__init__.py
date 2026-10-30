import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from .. import CONF_MHI_CNS_ID, MhiCnsComponent, mhi_cns_ns

CONF_TRACE_CAPTURE = "trace_capture"
CONF_DURATION = "duration"
MhiCnsTraceButton = mhi_cns_ns.class_(
    "MhiCnsTraceButton", button.Button, cg.Component, cg.Parented.template(MhiCnsComponent)
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_MHI_CNS_ID): cv.use_id(MhiCnsComponent),
    cv.Optional(CONF_TRACE_CAPTURE): button.button_schema(MhiCnsTraceButton, icon="mdi:console-line").extend({
        cv.Optional(CONF_DURATION, default="15s"): cv.positive_time_period_milliseconds,
    }),
})

async def to_code(config):
    if value := config.get(CONF_TRACE_CAPTURE):
        parent = await cg.get_variable(config[CONF_MHI_CNS_ID])
        var = await button.new_button(value)
        await cg.register_component(var, value)
        await cg.register_parented(var, parent)
        cg.add(var.set_duration(value[CONF_DURATION].total_milliseconds))
