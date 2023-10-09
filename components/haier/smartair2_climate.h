#pragma once

#include <chrono>
#include "haier_base.h"

namespace esphome {
namespace haier {

class Smartair2Climate : public HaierClimateBase {
 public:
  Smartair2Climate();
  Smartair2Climate(const Smartair2Climate &) = delete;
  Smartair2Climate &operator=(const Smartair2Climate &) = delete;
  ~Smartair2Climate();
  void dump_config() override;
  void set_alternative_swing_control(bool swing_control);

 protected:
  void set_handlers() override;
  void process_phase(std::chrono::steady_clock::time_point now) override;
  haier_protocol::HaierMessage get_control_message() override;
  void set_phase(HaierClimateBase::ProtocolPhases phase) override;
  // Answer and timeout handlers
  haier_protocol::HandlerError status_handler_(haier_protocol::FrameType request_type, haier_protocol::FrameType message_type, const uint8_t *data,
                                               size_t data_size);
  haier_protocol::HandlerError get_device_version_answer_handler_(haier_protocol::FrameType request_type, haier_protocol::FrameType message_type,
                                                                  const uint8_t *data, size_t data_size);
  haier_protocol::HandlerError get_device_id_answer_handler_(haier_protocol::FrameType request_type, haier_protocol::FrameType message_type,
                                                             const uint8_t *data, size_t data_size);
  haier_protocol::HandlerError report_network_status_answer_handler_(haier_protocol::FrameType request_type, haier_protocol::FrameType message_type,
                                                                     const uint8_t *data, size_t data_size);
  haier_protocol::HandlerError initial_messages_timeout_handler_(haier_protocol::FrameType message_type);
  // Helper functions
  haier_protocol::HandlerError process_status_message_(const uint8_t *packet, uint8_t size);
  std::unique_ptr<uint8_t[]> last_status_message_;
  unsigned int timeouts_counter_;
  bool use_alternative_swing_control_;
};

}  // namespace haier
}  // namespace esphome
