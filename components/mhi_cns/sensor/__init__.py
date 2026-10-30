import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_PERCENT,
    UNIT_SECOND,
)

from .. import CONF_MHI_CNS_ID, MhiCnsComponent

CONF_FRAME_ERRORS = "frame_errors"
CONF_VALID_FRAMES = "valid_frames"
CONF_SUCCESS_RATE = "success_rate"
CONF_LAST_VALID_FRAME_AGE = "last_valid_frame_age"
CONF_CLOCK_TIMEOUTS = "clock_timeouts"
CONF_SIGNATURE_ERRORS = "signature_errors"
CONF_CHECKSUM_ERRORS = "checksum_errors"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MHI_CNS_ID): cv.use_id(MhiCnsComponent),
        cv.Optional(CONF_FRAME_ERRORS): sensor.sensor_schema(
            icon="mdi:alert-circle-outline", accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING, entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_VALID_FRAMES): sensor.sensor_schema(
            icon="mdi:check-network-outline", accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING, entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_SUCCESS_RATE): sensor.sensor_schema(
            icon="mdi:percent-circle-outline", unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1, state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_LAST_VALID_FRAME_AGE): sensor.sensor_schema(
            icon="mdi:timer-sand", unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=1, state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_CLOCK_TIMEOUTS): sensor.sensor_schema(
            icon="mdi:timer-alert-outline", accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING, entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_SIGNATURE_ERRORS): sensor.sensor_schema(
            icon="mdi:file-alert-outline", accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING, entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_CHECKSUM_ERRORS): sensor.sensor_schema(
            icon="mdi:shield-alert-outline", accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING, entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MHI_CNS_ID])
    for key, setter in (
        (CONF_FRAME_ERRORS, parent.set_frame_errors_sensor),
        (CONF_VALID_FRAMES, parent.set_valid_frames_sensor),
        (CONF_SUCCESS_RATE, parent.set_success_rate_sensor),
        (CONF_LAST_VALID_FRAME_AGE, parent.set_last_valid_frame_age_sensor),
        (CONF_CLOCK_TIMEOUTS, parent.set_clock_timeouts_sensor),
        (CONF_SIGNATURE_ERRORS, parent.set_signature_errors_sensor),
        (CONF_CHECKSUM_ERRORS, parent.set_checksum_errors_sensor),
    ):
        if key in config:
            value = await sensor.new_sensor(config[key])
            cg.add(setter(value))
