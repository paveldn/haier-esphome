#pragma once

#include <chrono>
#include <set>
#include "esphome.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "Transport/protocol_transport.h"
#include "Protocol/haier_protocol.h"

namespace esphome {
namespace haier {

enum class AirflowVerticalDirection : uint8_t
{
  UP      = 0,
  CENTER  = 1,
  DOWN    = 2,
};

enum class AirflowHorizontalDirection : uint8_t
{
  LEFT    = 0,
  CENTER  = 1,
  RIGHT   = 2,
};

class HaierClimate :  public esphome::Component,
      public esphome::climate::Climate,
      public esphome::uart::UARTDevice,
      public HaierProtocol::ProtocolStream
{
public:
  HaierClimate() = delete;
  HaierClimate(const HaierClimate&) = delete;
  HaierClimate& operator=(const HaierClimate&) = delete;
  HaierClimate(esphome::uart::UARTComponent* parent);
  ~HaierClimate();
  void setup() override;
  void loop() override;
  void control(const esphome::climate::ClimateCall &call) override;
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::HARDWARE ; }
  void set_send_wifi_signal(bool sendWifi);
  void set_beeper_state(bool state);
  bool get_beeper_state() const;   
  void set_fahrenheit(bool fahrenheit);
  void set_outdoor_temperature_sensor(esphome::sensor::Sensor *sensor);
  void set_display_state(bool state);
  bool get_display_state() const;
  AirflowVerticalDirection get_vertical_airflow() const;
  void set_vertical_airflow(AirflowVerticalDirection direction);
  AirflowHorizontalDirection get_horizontal_airflow() const;
  void set_horizontal_airflow(AirflowHorizontalDirection direction);
  void set_supported_swing_modes(const std::set<esphome::climate::ClimateSwingMode> &modes);
  virtual size_t available() noexcept { return esphome::uart::UARTDevice::available(); };
  virtual size_t read_array(uint8_t* data, size_t len) noexcept { return esphome::uart::UARTDevice::read_array(data, len) ? len : 0; };
  virtual void write_array(const uint8_t* data, size_t len) noexcept { esphome::uart::UARTDevice::write_array(data, len);};
  bool can_send_message() const { return mHaierProtocol.getOutgoingQueueSize() == 0; };
protected:
  enum class ProtocolPhases
  {
    UNKNOWN                       = -1,
    // INITIALIZATION
    SENDING_INIT_1                = 0,
    WAITING_ANSWER_INIT_1         = 1,
    SENDING_INIT_2                = 2,
    WAITING_ANSWER_INIT_2         = 3,
    SENDING_FIRST_STATUS_REQUEST  = 4,
    WAITING_FIRST_STATUS_ANSWER   = 5,
    SENDING_ALARM_STATUS_REQUEST  = 6,
    WAITING_ALARM_STATUS_ANSWER   = 7,
    // FUNCTIONAL STATE
    IDLE                          = 8,
    SENDING_STATUS_REQUEST        = 9,
    WAITING_STATUS_ANSWER         = 10,
    SENDING_UPDATE_SIGNAL_REQUEST = 11,
    WAITING_UPDATE_SIGNAL_ANSWER  = 12,
    SENDING_SIGNAL_LEVEL          = 13,
    WAITING_SIGNAL_LEVEL_ANSWER   = 14,
    SENDING_CONTROL               = 15,
    WAITING_CONTROL_ANSWER        = 16,
  };
  esphome::climate::ClimateTraits traits() override;
  // Answers handlers
  HaierProtocol::HandlerError answer_preprocess(uint8_t requestMessageType, uint8_t expectedRequestMessageType, uint8_t answerMessageType, uint8_t expectedAnswerMessageType, ProtocolPhases expectedPhase);
  HaierProtocol::HandlerError get_device_version_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
  HaierProtocol::HandlerError get_device_id_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
  HaierProtocol::HandlerError status_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
  HaierProtocol::HandlerError get_management_information_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
  HaierProtocol::HandlerError report_network_status_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
  HaierProtocol::HandlerError get_alarm_status_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
  // Timeout handler
  HaierProtocol::HandlerError timeout_default_handler(uint8_t requestType);
  // Helper functions
  HaierProtocol::HandlerError process_status_message(const uint8_t* packet, uint8_t size);
  void send_message(const HaierProtocol::HaierMessage& command);
  const HaierProtocol::HaierMessage get_control_message();
  void set_phase(ProtocolPhases phase);
protected:
  struct HvacSettings
  {
    esphome::optional<esphome::climate::ClimateMode> mode;
    esphome::optional<esphome::climate::ClimateFanMode> fan_mode;
    esphome::optional<esphome::climate::ClimateSwingMode> swing_mode;
    esphome::optional<float> target_temperature;
    esphome::optional<esphome::climate::ClimatePreset> preset;
    bool valid;
    HvacSettings() : valid(false) {};
    void reset();
  };
  HaierProtocol::ProtocolHandler        mHaierProtocol;
  ProtocolPhases                        mPhase;
  uint8_t*                              mLastStatusMessage;
  uint8_t                               mFanModeFanSpeed;
  uint8_t                               mOtherModesFanSpeed;
  bool                                  mSendWifiSignal;
  bool                                  mBeeperStatus;
  bool                                  mUseFahrenheit;
  bool                                  mDisplayStatus;
  bool                                  mForceSendControl;
  bool                                  mForcedPublish;
  bool                                  mForcedRequestStatus;
  bool                                  mControlCalled;
  AirflowVerticalDirection              mVerticalDirection;
  AirflowHorizontalDirection            mHorizontalDirection;
  bool                                  mHvacHardwareInfoAvailable;
  std::string                           mHvacProtocolVersion;
  std::string                           mHvacSoftwareVersion;
  std::string                           mHvacHardwareVersion;
  std::string                           mHvacDeviceName;
  bool                                  mHvacFunctions[5];
  bool&                                 mUseCrc;
  uint8_t                               mActiveAlarms[8];
  esphome::sensor::Sensor*              mOutdoorSensor;
  esphome::climate::ClimateTraits       mTraits;
  HvacSettings                          mHvacSettings;
  std::chrono::steady_clock::time_point mLastRequestTimestamp;      // For interval between messages
  std::chrono::steady_clock::time_point mLastValidStatusTimestamp;  // For protocol timeout
  std::chrono::steady_clock::time_point mLastStatusRequest;         // To request AC status
  std::chrono::steady_clock::time_point mLastSignalRequest;         // To send WiFI signal level
  std::chrono::steady_clock::time_point mControlRequestTimestamp;   // To send control message
};

} // namespace haier
} // namespace esphome
