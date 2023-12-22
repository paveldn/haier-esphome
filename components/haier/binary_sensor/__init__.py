import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_FAN,
    ICON_RADIATOR,
)
from ..climate import HAIER_COMPONENT_SCHEMA, CONF_HAIER_ID

# Haier sensors
CONF_OUTDOOR_FAN_STATUS = "outdoor_fan_status"
CONF_DEFROST_STATUS = "defrost_status"
CONF_COMPRESSOR_STATUS = "compressor_status"
CONF_INDOOR_FAN_STATUS = "indoor_fan_status"
CONF_FOUR_WAY_VALVE_STATUS = "four_way_valve_status"
CONF_INDOOR_ELECTRIC_HEATING_STATUS = "indoor_electric_heating_status"

# Additional icons
ICON_SNOWFLAKE_THERMOMETER = "mdi:snowflake-thermometer"
ICON_HVAC = "mdi:hvac"
ICON_VALVE = "mdi:valve"

SENSOR_TYPES = {
    CONF_OUTDOOR_FAN_STATUS: binary_sensor.binary_sensor_schema(
        icon=ICON_FAN,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_DEFROST_STATUS: binary_sensor.binary_sensor_schema(
        icon=ICON_SNOWFLAKE_THERMOMETER,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_COMPRESSOR_STATUS: binary_sensor.binary_sensor_schema(
        icon=ICON_HVAC,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_INDOOR_FAN_STATUS: binary_sensor.binary_sensor_schema(
        icon=ICON_FAN,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_FOUR_WAY_VALVE_STATUS: binary_sensor.binary_sensor_schema(
        icon=ICON_VALVE,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_INDOOR_ELECTRIC_HEATING_STATUS: binary_sensor.binary_sensor_schema(
        icon=ICON_RADIATOR,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = HAIER_COMPONENT_SCHEMA.extend(
    {cv.Optional(type): schema for type, schema in SENSOR_TYPES.items()}
)

async def to_code(config):
    paren = await cg.get_variable(config[CONF_HAIER_ID])

    for type, _ in SENSOR_TYPES.items():
        if type in config:
            conf = config[type]
            sens = await binary_sensor.new_binary_sensor(conf)
            cg.add(getattr(paren, f"set_{type}_binary_sensor")(sens))