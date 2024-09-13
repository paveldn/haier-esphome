#include "vertical_airflow.h"
#include <protocol/haier_protocol.h>

namespace esphome {
namespace haier {

void VerticalAirflowSelect::control(const std::string &value) {
  hon_protocol::VerticalSwingMode state;
  if (value == "Auto") {
    state = hon_protocol::VerticalSwingMode::AUTO;
  } else if (value == "Health Up") {
    state = hon_protocol::VerticalSwingMode::HEALTH_UP;
  } else if (value == "Max Up") {
    state = hon_protocol::VerticalSwingMode::MAX_UP;
  } else if (value == "Up") {
    state = hon_protocol::VerticalSwingMode::UP;
  } else if (value == "Center") {
    state = hon_protocol::VerticalSwingMode::CENTER;
  } else if (value == "Down") {
    state = hon_protocol::VerticalSwingMode::DOWN;
  } else if (value == "Max Down") {
    state = hon_protocol::VerticalSwingMode::MAX_DOWN;
  } else if (value == "Health Down") {
    state = hon_protocol::VerticalSwingMode::HEALTH_DOWN;
  } else {
    ESP_LOGE("haier", "Invalid vertical airflow mode: %s", value.c_str());
    return;
  }
  const esphome::optional<hon_protocol::VerticalSwingMode> current_state = this->parent_->get_vertical_airflow();
  if (!current_state.has_value() || (current_state.value() != state)) {
    this->parent_->set_vertical_airflow(state);
  }
  this->publish_state(value);
}

}  // namespace haier
}  // namespace esphome
