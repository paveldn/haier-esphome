#include "health_mode_switch.h"

namespace esphome {
namespace haier {

void HealthModeSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_health_mode(state);
}

}  // namespace haier
}  // namespace esphome
