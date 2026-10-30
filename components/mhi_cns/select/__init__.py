import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select

from .. import CONF_MHI_CNS_ID, MhiCnsComponent, mhi_cns_ns

CONF_VERTICAL_VANES = "vertical_vanes"
CONF_HORIZONTAL_VANES = "horizontal_vanes"
VERTICAL_OPTIONS = ["Up", "Up/Center", "Center/Down", "Down", "Swing"]
HORIZONTAL_OPTIONS = ["Left", "Left/Center", "Center", "Center/Right", "Right", "Wide", "Spot", "Swing"]

MhiCnsVerticalVanesSelect = mhi_cns_ns.class_(
    "MhiCnsVerticalVanesSelect", select.Select, cg.Component, cg.Parented.template(MhiCnsComponent)
)
MhiCnsHorizontalVanesSelect = mhi_cns_ns.class_(
    "MhiCnsHorizontalVanesSelect", select.Select, cg.Component, cg.Parented.template(MhiCnsComponent)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MHI_CNS_ID): cv.use_id(MhiCnsComponent),
        cv.Optional(CONF_VERTICAL_VANES): select.select_schema(
            MhiCnsVerticalVanesSelect, icon="mdi:arrow-up-down"
        ),
        cv.Optional(CONF_HORIZONTAL_VANES): select.select_schema(
            MhiCnsHorizontalVanesSelect, icon="mdi:arrow-left-right"
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MHI_CNS_ID])
    if vertical := config.get(CONF_VERTICAL_VANES):
        var = await select.new_select(vertical, options=VERTICAL_OPTIONS)
        await cg.register_component(var, vertical)
        await cg.register_parented(var, parent)
    if horizontal := config.get(CONF_HORIZONTAL_VANES):
        var = await select.new_select(horizontal, options=HORIZONTAL_OPTIONS)
        await cg.register_component(var, horizontal)
        await cg.register_parented(var, parent)
