#include "beeper_switch.h"

namespace esphome {
namespace haier {

void BeeperSwitch::write_state(bool state) {
  if (state != this->state) {
    this->publish_state(state);
    this->parent_->set_beeper_state(state);
  }
}

}  // namespace haier
}  // namespace esphome
