#include "health_mode.h"

namespace esphome {
namespace haier {

void HealthModeSwitch::write_state(bool state) {
  this->publish_state(state);
}

}  // namespace haier
}  // namespace esphome
