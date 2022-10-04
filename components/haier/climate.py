import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, climate
from esphome import automation
from esphome.const import (
    CONF_BEEPER,
    CONF_ID,
    CONF_OFFSET,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_VISUAL,
    CONF_UART_ID,
    DEVICE_CLASS_TEMPERATURE,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

PROTOCOL_MIN_TEMPERATURE = 16.0
PROTOCOL_MAX_TEMPERATURE = 30.0

AUTO_LOAD = ["sensor"]
DEPENDENCIES = ["climate", "uart", "wifi"]
CONF_WIFI_SIGNAL = "wifi_signal"
CONF_OUTDOOR_TEMPERATURE = "outdoor_temperature"
CONF_VERTICAL_AIRFLOW = "vertical_airflow"
CONF_HORIZONTAL_AIRFLOW = "horizontal_airflow"

haier_ns = cg.esphome_ns.namespace("haier")
HaierClimate = haier_ns.class_("HaierClimate", climate.Climate, cg.Component)

AirflowVerticalDirection = haier_ns.enum("AirflowVerticalDirection")
AIRFLOW_VERTICAL_DIRECTION_OPTIONS = {
    "UP":       AirflowVerticalDirection.vdUp,
    "CENTER":   AirflowVerticalDirection.vdCenter,
    "DOWN": AirflowVerticalDirection.vdDown,
}

AirflowHorizontalDirection = haier_ns.enum("AirflowHorizontalDirection")
AIRFLOW_HORIZONTAL_DIRECTION_OPTIONS = {
    "LEFT": AirflowHorizontalDirection.hdLeft,
    "CENTER":   AirflowHorizontalDirection.hdCenter,
    "RIGHT":    AirflowHorizontalDirection.hdRight,
}

CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HaierClimate),
            cv.Optional(CONF_WIFI_SIGNAL, default=True): cv.boolean,
            cv.Optional(CONF_BEEPER, default=True): cv.boolean,
            cv.Optional(CONF_OUTDOOR_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_OFFSET, default=-20): cv.int_range(
                        min=-50, max=50
                    ), 
                }),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
)



# Actions
DisplayOnAction = haier_ns.class_("DisplayOnAction", automation.Action)
DisplayOffAction = haier_ns.class_("DisplayOffAction", automation.Action)
BeeperOnAction = haier_ns.class_("BeeperOnAction", automation.Action)
BeeperOffAction = haier_ns.class_("BeeperOffAction", automation.Action)
VerticalAirflowAction = haier_ns.class_("VerticalAirflowAction", automation.Action)
HorizontalAirflowAction = haier_ns.class_("HorizontalAirflowAction", automation.Action)

# Display on action
@automation.register_action(
    "climate.haier.display_on",
    DisplayOnAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(HaierClimate),
        }
    ),
)
async def haier_set_display_on_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var

# Display off action
@automation.register_action(
    "climate.haier.display_off",
    DisplayOffAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(HaierClimate),
        }
    ),
)
async def haier_set_display_off_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var

# Beeper on action
@automation.register_action(
    "climate.haier.beeper_on",
    BeeperOnAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(HaierClimate),
        }
    ),
)
async def haier_set_beeper_on_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var

# Beeper off action
@automation.register_action(
    "climate.haier.beeper_off",
    BeeperOffAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(HaierClimate),
        }
    ),
)
async def haier_set_beeper_off_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var

# Set vertiocal airflow direction action
@automation.register_action(
    "climate.haier.set_vertical_airflow",
    VerticalAirflowAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(HaierClimate),
            cv.Required(CONF_VERTICAL_AIRFLOW): cv.templatable(
                cv.enum(AIRFLOW_VERTICAL_DIRECTION_OPTIONS, upper=True)
            ), 
        }
    ),
)
async def haier_set_vertical_airflow_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_VERTICAL_AIRFLOW], args, AirflowVerticalDirection)
    cg.add(var.set_direction(template_))
    return var

# Set vertiocal airflow direction action
@automation.register_action(
    "climate.haier.set_horizontal_airflow",
    HorizontalAirflowAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(HaierClimate),
            cv.Required(CONF_HORIZONTAL_AIRFLOW): cv.templatable(
                cv.enum(AIRFLOW_HORIZONTAL_DIRECTION_OPTIONS, upper=True)
            ), 
        }
    ),
)
async def haier_set_horizontal_airflow_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_HORIZONTAL_AIRFLOW], args, AirflowHorizontalDirection)
    cg.add(var.set_direction(template_))
    return var


async def to_code(config):
    if CONF_VISUAL in config:
        visual_config = config[CONF_VISUAL]
        if CONF_MIN_TEMPERATURE in visual_config:
            min_temp = visual_config[CONF_MIN_TEMPERATURE]
            if min_temp < PROTOCOL_MIN_TEMPERATURE:
                raise cv.Invalid(f"Configured visual minimum temperature {min_temp} is lower than supported by Haier protocol {PROTOCOL_MIN_TEMPERATURE}")
        else:
            visual_config[CONF_MIN_TEMPERATURE] = PROTOCOL_MIN_TEMPERATURE
        if CONF_MAX_TEMPERATURE in visual_config:
            max_temp = visual_config[CONF_MAX_TEMPERATURE]
            if max_temp > PROTOCOL_MAX_TEMPERATURE:
                raise cv.Invalid(f"Configured visual maximum temperature {max_temp} is higher than supported by Haier protocol {PROTOCOL_MAX_TEMPERATURE}")
        else:
            visual_config[CONF_MAX_TEMPERATURE] = PROTOCOL_MAX_TEMPERATURE
    else:
        visual_config[CONF_MAX_TEMPERATURE] = {
            CONF_MIN_TEMPERATURE: PROTOCOL_MIN_TEMPERATURE,
            CONF_MAX_TEMPERATURE: PROTOCOL_MAX_TEMPERATURE,
            CONF_TEMPERATURE_STEP: 1
        }
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)
    cg.add(var.set_send_wifi_signal(config[CONF_WIFI_SIGNAL]))
    cg.add(var.set_beeper_echo(config[CONF_BEEPER]))
    if CONF_OUTDOOR_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_TEMPERATURE])
        cg.add(var.set_outdoor_temperature_sensor(sens))
        cg.add(var.set_outdoor_temperature_offset(config[CONF_OUTDOOR_TEMPERATURE][CONF_OFFSET]))

