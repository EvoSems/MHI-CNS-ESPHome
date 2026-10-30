import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_PERCENT,
    UNIT_SECOND,
    UNIT_CELSIUS,
    UNIT_AMPERE,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_HOUR,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_ENERGY,
    STATE_CLASS_TOTAL,
)

from .. import CONF_MHI_CNS_ID, MhiCnsComponent

CONF_FRAME_ERRORS = "frame_errors"
CONF_VALID_FRAMES = "valid_frames"
CONF_SUCCESS_RATE = "success_rate"
CONF_LAST_VALID_FRAME_AGE = "last_valid_frame_age"
CONF_CLOCK_TIMEOUTS = "clock_timeouts"
CONF_SIGNATURE_ERRORS = "signature_errors"
CONF_CHECKSUM_ERRORS = "checksum_errors"
CONF_COMMAND_FAILURES = "command_failures"
DATA_SENSORS = {
    "indoor_temperature": (UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, 1),
    "outdoor_temperature": (UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, 1),
    "return_air_temperature": (UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, 1),
    "current": (UNIT_AMPERE, DEVICE_CLASS_CURRENT, 2),
    "compressor_frequency": (UNIT_HERTZ, DEVICE_CLASS_FREQUENCY, 1),
    "energy_this_run": (UNIT_KILOWATT_HOURS, DEVICE_CLASS_ENERGY, 2),
    "indoor_coil_u_bend": (UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, 1),
    "indoor_coil_header": (UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, 1),
    "discharge_pipe_temperature": (UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, 1),
    "expansion_valve": ("pulse", None, 0),
    "indoor_fan_speed": (None, None, 0),
    "outdoor_fan_speed": (None, None, 0),
    "protection_code": (None, None, 0),
    "indoor_run_hours": (UNIT_HOUR, None, 0),
    "compressor_run_hours": (UNIT_HOUR, None, 0),
}

def data_sensor_schema(key, unit, device_class, decimals):
    kwargs = {
        "accuracy_decimals": decimals,
        "state_class": STATE_CLASS_TOTAL if key == "energy_this_run" else STATE_CLASS_MEASUREMENT,
    }
    if unit is not None:
        kwargs["unit_of_measurement"] = unit
    if device_class is not None:
        kwargs["device_class"] = device_class
    return sensor.sensor_schema(**kwargs)

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
        cv.Optional(CONF_COMMAND_FAILURES): sensor.sensor_schema(
            icon="mdi:message-alert-outline", accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING, entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        **{
            cv.Optional(key): data_sensor_schema(key, unit, device_class, decimals)
            for key, (unit, device_class, decimals) in DATA_SENSORS.items()
        },
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
        (CONF_COMMAND_FAILURES, parent.set_command_failures_sensor),
    ):
        if key in config:
            value = await sensor.new_sensor(config[key])
            cg.add(setter(value))
    for key in DATA_SENSORS:
        if key in config:
            value = await sensor.new_sensor(config[key])
            cg.add(getattr(parent, f"set_{key}_sensor")(value))
