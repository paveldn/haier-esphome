#include "horizontal_airflow.h"
#include <protocol/haier_protocol.h>

namespace esphome {
namespace haier {

void HorizontalAirflowSelect::control(const std::string &value) {
  hon_protocol::HorizontalSwingMode state;
  if (value == "Auto") {
    state = hon_protocol::HorizontalSwingMode::AUTO;
  } else if (value == "Max Left") {
    state = hon_protocol::HorizontalSwingMode::MAX_LEFT;
  } else if (value == "Left") {
    state = hon_protocol::HorizontalSwingMode::LEFT;
  } else if (value == "Center") {
    state = hon_protocol::HorizontalSwingMode::CENTER;
  } else if (value == "Right") {
    state = hon_protocol::HorizontalSwingMode::RIGHT;
  } else if (value == "Max Right") {
    state = hon_protocol::HorizontalSwingMode::MAX_RIGHT;
  } else {
    ESP_LOGE("haier", "Invalid horizontal airflow mode: %s", value.c_str());
    return;
  }
  const esphome::optional<hon_protocol::HorizontalSwingMode> current_state = this->parent_->get_horizontal_airflow();
  if (!current_state.has_value() || (current_state.value() != state)) {
    this->parent_->set_horizontal_airflow(state);
  }
  this->publish_state(value);
}

}  // namespace haier
}  // namespace esphome
