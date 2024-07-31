import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    CONF_BEEPER,
    CONF_DISPLAY,
    ENTITY_CATEGORY_CONFIG,
)
from ..climate import (
    CONF_HAIER_ID,
    HaierClimateBase,
    HonClimate,
    haier_ns,
)

CODEOWNERS = ["@paveldn"]
BeeperSwitch = haier_ns.class_("BeeperSwitch", switch.Switch)
HealthModeSwitch = haier_ns.class_("HealthModeSwitch", switch.Switch)
DisplaySwitch = haier_ns.class_("DisplaySwitch", switch.Switch)

# Haier switches
CONF_HEALTH_MODE = "health_mode"

# Additional icons
ICON_LEAF = "mdi:leaf"
ICON_LED_ON = "mdi:led-on"
ICON_VOLUME_HIGH = "mdi:volume-high"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAIER_ID): cv.use_id(HaierClimateBase),
        cv.Optional(CONF_DISPLAY): switch.switch_schema(
            DisplaySwitch,
            icon=ICON_LED_ON,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_HEALTH_MODE): switch.switch_schema(
            HealthModeSwitch,
            icon=ICON_LEAF,
        ),
        cv.Optional(CONF_BEEPER): switch.switch_schema(
            BeeperSwitch,
            icon=ICON_VOLUME_HIGH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)

from esphome.cpp_generator import MockObjClass


async def to_code(config):
    full_id, parent = await cg.get_variable_with_full_id(config[CONF_HAIER_ID])

    for switch_type in [CONF_DISPLAY, CONF_HEALTH_MODE]:
        if conf := config.get(switch_type):
            swtch = await switch.new_switch(conf)
            await cg.register_parented(swtch, parent)
    if conf := config.get(CONF_BEEPER):
        if full_id.type is HonClimate:
            swtch = await switch.new_switch(conf)
            await cg.register_parented(swtch, parent)
        else:
            raise ValueError("Beeper switch is only supported for hon climate")

