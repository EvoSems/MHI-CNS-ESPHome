import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@EvoSems"]
MULTI_CONF = False

CONF_MHI_CNS_ID = "mhi_cns_id"
CONF_FRAME_SIZE = "frame_size"
CONF_COMMAND_RETRIES = "command_retries"
CONF_SCK_PIN = "sck_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_MISO_PIN = "miso_pin"

mhi_cns_ns = cg.esphome_ns.namespace("mhi_cns")
MhiCnsComponent = mhi_cns_ns.class_("MhiCnsComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MhiCnsComponent),
        cv.Optional(CONF_FRAME_SIZE, default=20): cv.one_of(20, 33, int=True),
        cv.Optional(CONF_COMMAND_RETRIES, default=3): cv.int_range(min=1, max=10),
        cv.Optional(CONF_SCK_PIN, default=14): cv.int_range(min=0, max=16),
        cv.Optional(CONF_MOSI_PIN, default=13): cv.int_range(min=0, max=16),
        cv.Optional(CONF_MISO_PIN, default=12): cv.int_range(min=0, max=16),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_frame_size(config[CONF_FRAME_SIZE]))
    cg.add(var.set_command_retries(config[CONF_COMMAND_RETRIES]))
    cg.add(var.set_pins(config[CONF_SCK_PIN], config[CONF_MOSI_PIN], config[CONF_MISO_PIN]))
