#include "vertical_airflow.h"
#include <protocol/haier_protocol.h>

namespace esphome {
namespace haier {

void VerticalAirflowSelect::control(const std::string &value) {
  this->publish_state(value);
}

}  // namespace haier
}  // namespace esphome
