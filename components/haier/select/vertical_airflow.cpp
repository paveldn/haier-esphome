#include "vertical_airflow.h"
#include <protocol/haier_protocol.h>

namespace esphome {
namespace haier {

void VerticalAirflowSelect::control(const std::string &value) {
  hon_protocol::VerticalSwingMode state;
  static const std::vector<std::string> options = this->traits.get_options();
  auto item_it = std::find(options.begin(), options.end(), value);
  if (item_it == options.end()) {
    ESP_LOGE("haier", "Invalid vertical airflow mode: %s", value.c_str());
    return;
  }
  state = VERTICAL_SWING_MODES_ORDER[item_it - options.begin()];
  const esphome::optional<hon_protocol::VerticalSwingMode> current_state = this->parent_->get_vertical_airflow();
  if (!current_state.has_value() || (current_state.value() != state)) {
    this->parent_->set_vertical_airflow(state);
  }
  this->publish_state(value);
}

}  // namespace haier
}  // namespace esphome
