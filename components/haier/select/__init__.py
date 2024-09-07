import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import (
    CONF_OPTIONS,
    ENTITY_CATEGORY_CONFIG,
)
from ..climate import (
    CONF_HAIER_ID,
    HonClimate,
    haier_ns,
    hon_protocol_ns,
    CONF_VERTICAL_AIRFLOW,
    CONF_HORIZONTAL_AIRFLOW,
)

CODEOWNERS = ["@paveldn"]
VerticalAirflowSelect = haier_ns.class_("VerticalAirflowSelect", select.Select)
HorizontalAirflowSelect = haier_ns.class_("HorizontalAirflowSelect", select.Select)

VerticalSwingMode = hon_protocol_ns.enum("VerticalSwingMode", True)
HorizontalSwingMode = hon_protocol_ns.enum("HorizontalSwingMode", True)

# Additional icons
ICON_ARROW_HORIZONTAL =  "mdi:arrow-expand-horizontal"
ICON_ARROW_VERTICAL = "mdi:arrow-expand-vertical"

AIRFLOW_VERTICAL_DIRECTION_OPTIONS = {
    "Auto": VerticalSwingMode.AUTO,
    "Health Up": VerticalSwingMode.HEALTH_UP,
    "Max Up": VerticalSwingMode.MAX_UP,
    "Up": VerticalSwingMode.UP,
    "Center": VerticalSwingMode.CENTER,
    "Down": VerticalSwingMode.DOWN,
    "Max Down": VerticalSwingMode.MAX_DOWN,
    "Health Down": VerticalSwingMode.HEALTH_DOWN,
}

AIRFLOW_HORIZONTAL_DIRECTION_OPTIONS = {
    "Auto": HorizontalSwingMode.AUTO,
    "Max Left": HorizontalSwingMode.MAX_LEFT,
    "Left": HorizontalSwingMode.LEFT,
    "Center": HorizontalSwingMode.CENTER,
    "Right": HorizontalSwingMode.RIGHT,
    "Max Right": HorizontalSwingMode.MAX_RIGHT,
}

#def check_airflow_map(value, options_map):
#    cv.check_not_templatable(value)
#    option_key = cv.All(cv.string_strict)
#    option_value =
#    if value not in AIRFLOW_VERTICAL_DIRECTION_OPTIONS:
#        raise cv.Invalid(f"Invalid airflow value {value}, must be one of: {', '.join(AIRFLOW_VERTICAL_DIRECTION_OPTIONS)}")
#    return value

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAIER_ID): cv.use_id(HonClimate),
        cv.Optional(CONF_VERTICAL_AIRFLOW): select.select_schema(
            VerticalAirflowSelect,
            icon=ICON_ARROW_VERTICAL,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_HORIZONTAL_AIRFLOW): select.select_schema(
            HorizontalAirflowSelect,
            icon=ICON_ARROW_HORIZONTAL,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)

async def to_code(config):
    full_id, parent = await cg.get_variable_with_full_id(config[CONF_HAIER_ID])

    if conf := config.get(CONF_VERTICAL_AIRFLOW):
        sel_var = await select.new_select(conf, options=list(AIRFLOW_VERTICAL_DIRECTION_OPTIONS.keys()))
        await cg.register_parented(sel_var, parent)
        cg.add(getattr(parent, f"set_{CONF_VERTICAL_AIRFLOW}_select")(sel_var))

    if conf := config.get(CONF_HORIZONTAL_AIRFLOW):
        sel_var = await select.new_select(conf, options=list(AIRFLOW_HORIZONTAL_DIRECTION_OPTIONS.keys()))
        await cg.register_parented(sel_var, parent)
        cg.add(getattr(parent, f"set_{CONF_HORIZONTAL_AIRFLOW}_select")(sel_var))


