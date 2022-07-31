import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, climate
from esphome import automation
from esphome.const import (
    CONF_BEEPER,
    CONF_ID,
    CONF_OFFSET,
    CONF_UART_ID,
    DEVICE_CLASS_TEMPERATURE,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

AUTO_LOAD = ["sensor"]
DEPENDENCIES = ["climate", "uart", "wifi"]
CONF_WIFI_SIGNAL = "wifi_signal"
CONF_OUTDOOR_TEMPERATURE = "outdoor_temperature"
CONF_USE_FAHRENHEIT = "use_fahrenheit"

haier_ns = cg.esphome_ns.namespace("haier")
HaierClimate = haier_ns.class_("HaierClimate", climate.Climate, cg.Component)

CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HaierClimate),
            cv.Optional(CONF_WIFI_SIGNAL, default=True): cv.boolean,
            cv.Optional(CONF_BEEPER, default=True): cv.boolean,
            cv.Optional(CONF_USE_FAHRENHEIT, default=False): cv.boolean,
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

async def to_code(config):
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)
    cg.add(var.set_send_wifi_signal(config[CONF_WIFI_SIGNAL]))
    cg.add(var.set_beeper_echo(config[CONF_BEEPER]))
    cg.add(var.set_fahrenheit(config[CONF_USE_FAHRENHEIT]))
    if CONF_OUTDOOR_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_TEMPERATURE])
        cg.add(var.set_outdoor_temperature_sensor(sens))
        cg.add(var.set_outdoor_temperature_offset(config[CONF_OUTDOOR_TEMPERATURE][CONF_OFFSET]))

