import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
)
from ..climate import (
    CONF_BEEPER,
    CONF_DISPLAY,
    CONF_HAIER_ID,
    haier_ns,
    HaierClimateBase,
    HonClimate,
)

BeeperSwitch = haier_ns.class_("BeeperSwitch", cg.Component)
DisplaySwitch = haier_ns.class_("DisplaySwitch", cg.Component)
HealthModeSwitch = haier_ns.class_("HealthModeSwitch", cg.Component)

CONF_HEALTH_MODE = "health_mode"

# Additional icons
ICON_LED_ON = "mdi:led-on"
ICON_LEAF = "mdi:leaf"
ICON_VOLUME_HIGH = "mdi:volume-high"

CONFIG_SCHEMA = cv.All(
    {
        cv.Required(CONF_HAIER_ID): cv.use_id(HaierClimateBase),
        cv.Optional(CONF_DISPLAY): switch.switch_schema(
            DisplaySwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_LED_ON,
            default_restore_mode = "DISABLED",
        ),
        cv.Optional(CONF_HEALTH_MODE): switch.switch_schema(
            HealthModeSwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_LEAF,
            default_restore_mode = "DISABLED",
        ),
        cv.Optional(CONF_BEEPER): switch.switch_schema(
            BeeperSwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_VOLUME_HIGH,
            default_restore_mode = "DISABLED",
        ),
    }
)

async def to_code(config):
    full_id, paren = await cg.get_variable_with_full_id(config[CONF_HAIER_ID])
    if conf := config.get(CONF_DISPLAY):
        var = await switch.new_switch(conf)
        await cg.register_parented(var, config[CONF_HAIER_ID])
        cg.add(paren.set_display_switch(var))
    if conf := config.get(CONF_HEALTH_MODE):
        var = await switch.new_switch(conf)
        await cg.register_parented(var, config[CONF_HAIER_ID])
        cg.add(paren.set_health_mode_switch(var))
    if conf := config.get(CONF_BEEPER):
        if str(full_id.type) == str(HonClimate):
            var = await switch.new_switch(conf)
            await cg.register_parented(var, config[CONF_HAIER_ID])
            cg.add(paren.set_beeper_switch(var))
        else:
            raise cv.Invalid(f"{CONF_BEEPER} allowed only with hOn climate.")

