#include "display.h"

namespace esphome {
namespace haier {

void DisplaySwitch::write_state(bool state) {
  this->publish_state(state);
}

}  // namespace haier
}  // namespace esphome
