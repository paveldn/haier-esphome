#include "esphome.h"
#include <chrono>
#include <string>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/wifi/wifi_component.h"
#include "haier_climate.h"
#include "haier_packet.h"
#include "Protocol/haier_protocol.h"

using namespace esphome::climate;
using namespace esphome::uart;

#define TAG "haier.climate"

#ifndef ESPHOME_LOG_LEVEL
#warning "No ESPHOME_LOG_LEVEL defined!"
#endif

namespace esphome {
namespace haier {

constexpr size_t COMMUNICATION_TIMEOUT_MS =         60000;
constexpr size_t STATUS_REQUEST_INTERVAL_MS =       5000;
constexpr size_t DEFAULT_MESSAGES_INTERVAL_MS =     2000;
constexpr size_t CONTROL_MESSAGES_INTERVAL_MS =     400;
constexpr size_t SIGNAL_LEVEL_UPDATE_INTERVAL_MS =  10000;
constexpr size_t CONTROL_TIMEOUT_MS =               7000;
constexpr int PROTOCOL_OUTDOOR_TEMPERATURE_OFFSET = -64;

hon::VerticalSwingMode get_vertical_swing_mode(AirflowVerticalDirection direction)
{
  switch (direction)
  {
    case AirflowVerticalDirection::UP:
      return hon::VerticalSwingMode::UP;
    case AirflowVerticalDirection::DOWN:
      return hon::VerticalSwingMode::DOWN;
    default:
      return hon::VerticalSwingMode::CENTER;
  }
}

hon::HorizontalSwingMode get_horizontal_swing_mode(AirflowHorizontalDirection direction)
{
  switch (direction)
  {
    case AirflowHorizontalDirection::LEFT:
      return hon::HorizontalSwingMode::LEFT;
    case AirflowHorizontalDirection::RIGHT:
      return hon::HorizontalSwingMode::RIGHT;
    default:
      return hon::HorizontalSwingMode::CENTER;
  }
}

HaierClimate::HaierClimate(UARTComponent* parent) :
              Component(),
              UARTDevice(parent),
              haier_protocol_(*this),
              protocol_phase_(ProtocolPhases::SENDING_INIT_1),
              fan_mode_speed_((uint8_t)hon::FanMode::MID),
              other_modes_fan_speed_((uint8_t)hon::FanMode::AUTO),
              send_wifi_signal_(false),
              beeper_status_(true),
              display_status_(true),
              force_send_control_(false),
              forced_publish_(false),
              forced_request_status_(false),
              control_called_(false),
              hvac_hardware_info_available_(false),
              hvac_functions_{false, false, false, false, false},
              use_crc_(hvac_functions_[2]),
              active_alarms_{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
              outdoor_sensor_(nullptr)
{
  last_status_message_ = new uint8_t[sizeof(hon::HaierPacketControl)];
  traits_ = climate::ClimateTraits();
  traits_.set_supported_modes(
  {
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_FAN_ONLY,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_AUTO
  });
  traits_.set_supported_fan_modes(
  {
    climate::CLIMATE_FAN_AUTO,
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH,
  });
  traits_.set_supported_swing_modes(
  {
    climate::CLIMATE_SWING_OFF,
    climate::CLIMATE_SWING_BOTH,
    climate::CLIMATE_SWING_VERTICAL,
    climate::CLIMATE_SWING_HORIZONTAL
  });
  traits_.set_supported_presets(
  {
    climate::CLIMATE_PRESET_NONE,
    climate::CLIMATE_PRESET_ECO,
    climate::CLIMATE_PRESET_BOOST,
    climate::CLIMATE_PRESET_SLEEP,
  });
  traits_.set_supports_current_temperature(true);
  vertical_direction_ = AirflowVerticalDirection::CENTER;
  horizontal_direction_ = AirflowHorizontalDirection::CENTER;
  //mLastValidStatusTimestamp = std::chrono::steady_clock::now();
}

HaierClimate::~HaierClimate()
{
  delete[] last_status_message_;
}

void HaierClimate::set_phase(ProtocolPhases phase)
{
  if (protocol_phase_ != phase)
  {
    ESP_LOGV(TAG, "Phase transition: %d => %d", protocol_phase_, phase);
    protocol_phase_ = phase;
  }
}

void HaierClimate::set_send_wifi_signal(bool sendWifi) 
{
  send_wifi_signal_ = sendWifi; 
}

void HaierClimate::set_beeper_state(bool state)
{
  beeper_status_ = state;
}

bool HaierClimate::get_beeper_state() const
{
  return beeper_status_;
}

bool HaierClimate::get_display_state() const
{
  return display_status_;
}

void HaierClimate::set_display_state(bool state)
{
  if (display_status_ != state)
  {
    display_status_ = state;
    force_send_control_ = true;
  }
}

void HaierClimate::set_outdoor_temperature_sensor(esphome::sensor::Sensor *sensor) 
{
  outdoor_sensor_ = sensor; 
}

AirflowVerticalDirection HaierClimate::get_vertical_airflow() const
{
  return vertical_direction_;
};

void HaierClimate::set_vertical_airflow(AirflowVerticalDirection direction)
{
  if (direction > AirflowVerticalDirection::DOWN)
    vertical_direction_ = AirflowVerticalDirection::CENTER;
  else
    vertical_direction_ = direction;
  force_send_control_ = true;
}

AirflowHorizontalDirection HaierClimate::get_horizontal_airflow() const
{
  return horizontal_direction_;
}

void HaierClimate::set_horizontal_airflow(AirflowHorizontalDirection direction)
{
  if (direction > AirflowHorizontalDirection::RIGHT)
    horizontal_direction_ = AirflowHorizontalDirection::CENTER;
  else
    horizontal_direction_ = direction;
  force_send_control_ = true;
}

void HaierClimate::set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes)
{
  traits_.set_supported_swing_modes(modes);
  traits_.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);   // Always available
  traits_.add_supported_swing_mode(CLIMATE_SWING_VERTICAL);       // Always available
}

void esphome_logger(HaierProtocol::HaierLogLevel level, const char* tag, const char* message)
{
  switch (level)
  {
    case HaierProtocol::HaierLogLevel::llError:
      esp_log_printf_(ESPHOME_LOG_LEVEL_ERROR, tag, __LINE__, message);
      break;
    case HaierProtocol::HaierLogLevel::llWarning:
      esp_log_printf_(ESPHOME_LOG_LEVEL_WARN, tag, __LINE__, message);
      break;
    case HaierProtocol::HaierLogLevel::llInfo:
      esp_log_printf_(ESPHOME_LOG_LEVEL_INFO, tag, __LINE__, message);
      break;
    case HaierProtocol::HaierLogLevel::llDebug:
      esp_log_printf_(ESPHOME_LOG_LEVEL_DEBUG, tag, __LINE__, message);
      break;
    case HaierProtocol::HaierLogLevel::llVerbose:
      esp_log_printf_(ESPHOME_LOG_LEVEL_VERBOSE, tag, __LINE__, message);
      break;
    default:
      // Just ignore everything else
      break;
  }
}

HaierProtocol::HandlerError HaierClimate::answer_preprocess(uint8_t requestMessageType, uint8_t expectedRequestMessageType, uint8_t answerMessageType, uint8_t expectedAnswerMessageType, ProtocolPhases expectedPhase)
{
  HaierProtocol::HandlerError result = HaierProtocol::HandlerError::heOK;
  if ((expectedRequestMessageType != (uint8_t)hon::FrameType::NO_COMMAND) && (requestMessageType != expectedRequestMessageType))
    result = HaierProtocol::HandlerError::heUnsuportedMessage;
  if ((expectedAnswerMessageType != (uint8_t)hon::FrameType::NO_COMMAND) && (answerMessageType != expectedAnswerMessageType))
    result = HaierProtocol::HandlerError::heUnsuportedMessage;
  if ((expectedPhase != ProtocolPhases::UNKNOWN) && (expectedPhase != protocol_phase_))
    result = HaierProtocol::HandlerError::heUnexpectedMessage;
  if (answerMessageType == (uint8_t)hon::FrameType::INVALID)
    result = HaierProtocol::HandlerError::heInvalidAnswer;
  return result;
}

HaierProtocol::HandlerError HaierClimate::get_device_version_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  HaierProtocol::HandlerError result = answer_preprocess(requestType, (uint8_t)hon::FrameType::GET_DEVICE_VERSION, messageType, (uint8_t)hon::FrameType::GET_DEVICE_VERSION_RESPONSE, ProtocolPhases::WAITING_ANSWER_INIT_1);
  if (result == HaierProtocol::HandlerError::heOK)
  {
    if (dataSize < sizeof(hon::DeviceVersionAnswer))
    {
      // Wrong structure
      set_phase(ProtocolPhases::SENDING_INIT_1);
      return HaierProtocol::HandlerError::heWrongMessageStructure;
    }
    // All OK
    hon::DeviceVersionAnswer* answr = (hon::DeviceVersionAnswer*)data;
    char tmp[9];
    tmp[8] = 0;
    strncpy(tmp, answr->protocol_version, 8);
    hvac_protocol_version_ = std::string(tmp);
    strncpy(tmp, answr->software_version, 8);
    hvac_software_version_ = std::string(tmp);
    strncpy(tmp, answr->hardware_version, 8);
    hvac_hardware_version_ = std::string(tmp);
    strncpy(tmp, answr->device_name, 8);
    hvac_device_name_ = std::string(tmp);
    hvac_functions_[0] = (answr->functions[1] & 0x01) != 0;      // interactive mode support
    hvac_functions_[1] = (answr->functions[1] & 0x02) != 0;      // master-slave mode support
    hvac_functions_[2] = (answr->functions[1] & 0x04) != 0;      // crc support
    hvac_functions_[3] = (answr->functions[1] & 0x08) != 0;      // multiple AC support
    hvac_functions_[4] = (answr->functions[1] & 0x20) != 0;      // roles support
    hvac_hardware_info_available_ = true;
    set_phase(ProtocolPhases::SENDING_INIT_2);
    return result;
  }
  else
  {
    set_phase((protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE : ProtocolPhases::SENDING_INIT_1);
    return result;
  }
}

HaierProtocol::HandlerError HaierClimate::get_device_id_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  HaierProtocol::HandlerError result = answer_preprocess(requestType, (uint8_t)hon::FrameType::GET_DEVICE_ID, messageType, (uint8_t)hon::FrameType::GET_DEVICE_ID_RESPONSE, ProtocolPhases::WAITING_ANSWER_INIT_2);
  if (result == HaierProtocol::HandlerError::heOK)
  {
    set_phase(ProtocolPhases::SENDING_FIRST_STATUS_REQUEST);
    return result;
  }
  else
  {
    set_phase((protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE : ProtocolPhases::SENDING_INIT_1);
    return result;
  }
}

HaierProtocol::HandlerError HaierClimate::status_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  HaierProtocol::HandlerError result = answer_preprocess(requestType, (uint8_t)hon::FrameType::CONTROL, messageType, (uint8_t)hon::FrameType::STATUS, ProtocolPhases::UNKNOWN);
  if (result == HaierProtocol::HandlerError::heOK)
  {
    result = process_status_message(data, dataSize);
    if (result != HaierProtocol::HandlerError::heOK)
    {
      ESP_LOGW(TAG, "Error %d while parsing Status packet", (int)result);
      set_phase((protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE : ProtocolPhases::SENDING_INIT_1);
    }
    else
    {
      if (dataSize >= sizeof(hon::HaierPacketControl) + 2)
      {
        memcpy(last_status_message_, data + 2, sizeof(hon::HaierPacketControl));
        ESP_LOGV(TAG, "Last status message <= %s", buf2hex(last_status_message_, sizeof(hon::HaierPacketControl)).c_str());
      }
      else
        ESP_LOGW(TAG, "Status packet too small: %d (should be >= %d)", dataSize, sizeof(hon::HaierPacketControl));
      if (protocol_phase_ == ProtocolPhases::WAITING_FIRST_STATUS_ANSWER)
      {
        ESP_LOGI(TAG, "First HVAC status received");
        set_phase(ProtocolPhases::SENDING_ALARM_STATUS_REQUEST);
      }
      else if (protocol_phase_ == ProtocolPhases::WAITING_STATUS_ANSWER)
        set_phase(ProtocolPhases::IDLE);
      else if (protocol_phase_ == ProtocolPhases::WAITING_CONTROL_ANSWER)
      {
        set_phase(ProtocolPhases::IDLE); 
        force_send_control_ = false;
        if (hvac_settings_.valid)
          hvac_settings_.reset();
      }
    }
    return result;
  }
  else
  {
    set_phase((protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE : ProtocolPhases::SENDING_INIT_1);
    return result;
  }
}

HaierProtocol::HandlerError HaierClimate::get_management_information_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  HaierProtocol::HandlerError result = answer_preprocess(requestType, (uint8_t)hon::FrameType::GET_MANAGEMENT_INFORMATION, messageType, (uint8_t)hon::FrameType::GET_MANAGEMENT_INFORMATION_RESPONSE, ProtocolPhases::WAITING_UPDATE_SIGNAL_ANSWER);
  if (result == HaierProtocol::HandlerError::heOK)
  {
    set_phase(ProtocolPhases::SENDING_SIGNAL_LEVEL);
    return result;
  }
  else
  {
    set_phase(ProtocolPhases::IDLE);
    return result;
  }
}

HaierProtocol::HandlerError HaierClimate::report_network_status_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  HaierProtocol::HandlerError result = answer_preprocess(requestType, (uint8_t)hon::FrameType::REPORT_NETWORK_STATUS, messageType, (uint8_t)hon::FrameType::CONFIRM, ProtocolPhases::WAITING_SIGNAL_LEVEL_ANSWER);
  if (result == HaierProtocol::HandlerError::heOK)
  {
    set_phase(ProtocolPhases::IDLE);
    return result;
  }
  else
  {
    set_phase(ProtocolPhases::IDLE);
    return result;
  }
}

HaierProtocol::HandlerError HaierClimate::get_alarm_status_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  if (requestType == (uint8_t)hon::FrameType::GET_ALARM_STATUS)
  {
    if (messageType != (uint8_t)hon::FrameType::GET_ALARM_STATUS_RESPONSE)
    {
      // Unexpected answer to request
      set_phase(ProtocolPhases::IDLE);
      return HaierProtocol::HandlerError::heUnsuportedMessage;
    }
    if (protocol_phase_ != ProtocolPhases::WAITING_ALARM_STATUS_ANSWER)
    {
      // Don't expect this answer now
      set_phase(ProtocolPhases::IDLE);
      return HaierProtocol::HandlerError::heUnexpectedMessage;
    }
    memcpy(active_alarms_, data + 2, 8);
    set_phase(ProtocolPhases::IDLE);        return HaierProtocol::HandlerError::heOK;
  }
  else
  {
    // Answer to unexpected command,
    // Shouldn't get here!
    set_phase(ProtocolPhases::IDLE);
    return HaierProtocol::HandlerError::heUnsuportedMessage;
  }
}

HaierProtocol::HandlerError HaierClimate::timeout_default_handler(uint8_t requestType)
{
  ESP_LOGW(TAG, "Answer timeout for command %02X, phase %d", requestType, protocol_phase_);
  if (protocol_phase_ > ProtocolPhases::IDLE)
    set_phase(ProtocolPhases::IDLE);
  else
    set_phase(ProtocolPhases::SENDING_INIT_1);
  return HaierProtocol::HandlerError::heOK;
}

void HaierClimate::setup()
{
  HaierProtocol::setLogHandler(esphome_logger);
  ESP_LOGI(TAG, "Haier initialization...");
  // Set timestamp here to give AC time to boot
  last_request_timestamp_ = std::chrono::steady_clock::now();
  set_phase(ProtocolPhases::SENDING_INIT_1);
  // Set handlers
#define SET_HANDLER(command, handler) this->haier_protocol_.setAnswerHandler((uint8_t)(command), std::bind(&handler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  SET_HANDLER(hon::FrameType::GET_DEVICE_VERSION, esphome::haier::HaierClimate::get_device_version_answer_handler);
  SET_HANDLER(hon::FrameType::GET_DEVICE_ID, esphome::haier::HaierClimate::get_device_id_answer_handler);
  SET_HANDLER(hon::FrameType::CONTROL, esphome::haier::HaierClimate::status_handler);
  SET_HANDLER(hon::FrameType::GET_MANAGEMENT_INFORMATION, esphome::haier::HaierClimate::get_management_information_answer_handler);
  SET_HANDLER(hon::FrameType::GET_ALARM_STATUS, esphome::haier::HaierClimate::get_alarm_status_answer_handler);
  SET_HANDLER(hon::FrameType::REPORT_NETWORK_STATUS, esphome::haier::HaierClimate::report_network_status_answer_handler);
#undef SET_HANDLER
  this->haier_protocol_.setDefaultTimeoutHandler(std::bind(&esphome::haier::HaierClimate::timeout_default_handler, this, std::placeholders::_1));
}

void HaierClimate::dump_config()
{
  LOG_CLIMATE("", "Haier hOn Climate", this);
  if (hvac_hardware_info_available_)
  {
    ESP_LOGCONFIG(TAG, "  Device protocol version: %s",
      hvac_protocol_version_.c_str());
    ESP_LOGCONFIG(TAG, "  Device software version: %s", 
      hvac_software_version_.c_str());
    ESP_LOGCONFIG(TAG, "  Device hardware version: %s",
      hvac_hardware_version_.c_str());
    ESP_LOGCONFIG(TAG, "  Device name: %s", 
      hvac_device_name_.c_str());
    ESP_LOGCONFIG(TAG, "  Device features:%s%s%s%s%s",
      (hvac_functions_[0] ? " interactive" : ""), 
      (hvac_functions_[1] ? " master-slave" : ""),
      (hvac_functions_[2] ? " crc" : ""), 
      (hvac_functions_[3] ? " multinode" : ""),
      (hvac_functions_[4] ? " role" : ""));
    ESP_LOGCONFIG(TAG, "  Active alarms: %s", 
      buf2hex(active_alarms_, sizeof(active_alarms_)).c_str());
#if (HAIER_LOG_LEVEL > 4)
    ESP_LOGCONFIG(TAG, "  Protocol phase: %d",
      protocol_phase_);
#endif
  }
}

void HaierClimate::loop()
{
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_valid_status_timestamp_).count() > COMMUNICATION_TIMEOUT_MS)
  {
    if (protocol_phase_ > ProtocolPhases::IDLE)
    {
      // No status too long, reseting protocol
      ESP_LOGW(TAG, "Communication timeout, reseting protocol");
      last_valid_status_timestamp_ = now;
      force_send_control_ = false;
      if (hvac_settings_.valid)
        hvac_settings_.reset();
      set_phase(ProtocolPhases::SENDING_INIT_1);
      return;
    }
    else
      // No need to reset protocol if we didn't pass initialization phase
      last_valid_status_timestamp_ = now;
  };
  if (hvac_settings_.valid || force_send_control_)
  {
    // If control message is pending we should send it ASAP unless we are in initialisation procedure or waiting for an answer
    if (    (protocol_phase_ == ProtocolPhases::IDLE) ||
        (protocol_phase_ == ProtocolPhases::SENDING_STATUS_REQUEST) || 
        (protocol_phase_ == ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST) ||
        (protocol_phase_ == ProtocolPhases::SENDING_SIGNAL_LEVEL))
    {
      ESP_LOGV(TAG, "Control packet is pending...");
      set_phase(ProtocolPhases::SENDING_CONTROL);
    }
  }
  switch (protocol_phase_)
  {
    case ProtocolPhases::SENDING_INIT_1:
      if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_timestamp_).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        hvac_hardware_info_available_ = false;
        // Indicate device capabilities:
        // bit 0 - if 1 module support interactive mode
        // bit 1 - if 1 module support master-slave mode
        // bit 2 - if 1 module support crc
        // bit 3 - if 1 module support multiple devices
        // bit 4..bit 15 - not used
        uint8_t module_capabilities[2] = { 0b00000000, 0b00000111 };     
        static const HaierProtocol::HaierMessage deviceVersion_request((uint8_t)hon::FrameType::GET_DEVICE_VERSION, module_capabilities, sizeof(module_capabilities));
        send_message(deviceVersion_request);
        set_phase(ProtocolPhases::WAITING_ANSWER_INIT_1);
      }
      break;
    case ProtocolPhases::SENDING_INIT_2:
      if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_timestamp_).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static const HaierProtocol::HaierMessage deviceId_request((uint8_t)hon::FrameType::GET_DEVICE_ID);
        send_message(deviceId_request);
        set_phase(ProtocolPhases::WAITING_ANSWER_INIT_2);
      }
      break;
    case ProtocolPhases::SENDING_FIRST_STATUS_REQUEST:
    case ProtocolPhases::SENDING_STATUS_REQUEST:
      if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_timestamp_).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static const HaierProtocol::HaierMessage status_request((uint8_t)hon::FrameType::CONTROL, (uint16_t)hon::SubcomandsControl::GET_USER_DATA);
        send_message(status_request);
        last_status_request_ = now;
        set_phase((ProtocolPhases)((uint8_t)protocol_phase_ + 1));
      }
      break;
    case ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST:
      if (!send_wifi_signal_)
        set_phase(ProtocolPhases::IDLE);
      else if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_timestamp_).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static const HaierProtocol::HaierMessage update_signal_request((uint8_t)hon::FrameType::GET_MANAGEMENT_INFORMATION);
        send_message(update_signal_request);
        last_signal_request_ = now;
        set_phase(ProtocolPhases::WAITING_UPDATE_SIGNAL_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_SIGNAL_LEVEL:
      if (!send_wifi_signal_)
        set_phase(ProtocolPhases::IDLE);
      else if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_timestamp_).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static uint8_t wifi_status_data[4] = { 0x00, 0x00, 0x00, 0x00 };
        if (wifi::global_wifi_component->is_connected())
        {
          wifi_status_data[1] = 0;
          int8_t _rssi = wifi::global_wifi_component->wifi_rssi();
          wifi_status_data[3] = uint8_t((128 + _rssi) / 1.28f);
          ESP_LOGD(TAG, "WiFi signal is: %ddBm => %d%%", _rssi, wifi_status_data[3]);
        }
        else
        {
          ESP_LOGD(TAG, "WiFi is not connected");
          wifi_status_data[1] = 1;
          wifi_status_data[3] = 0;
        }
        HaierProtocol::HaierMessage wifi_status_request((uint8_t)hon::FrameType::REPORT_NETWORK_STATUS, wifi_status_data, sizeof(wifi_status_data));
        send_message(wifi_status_request);
        set_phase(ProtocolPhases::WAITING_SIGNAL_LEVEL_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_ALARM_STATUS_REQUEST:
      if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_timestamp_).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static const HaierProtocol::HaierMessage alarm_status_request((uint8_t)hon::FrameType::GET_ALARM_STATUS);
        send_message(alarm_status_request);
        set_phase(ProtocolPhases::WAITING_ALARM_STATUS_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_CONTROL:
      if (control_called_)
      {
        control_request_timestamp_ = now;
        control_called_ = false;
      }
      if (std::chrono::duration_cast<std::chrono::milliseconds>(now - control_request_timestamp_).count() > CONTROL_TIMEOUT_MS)
      {
        ESP_LOGW(TAG, "Sending control packet timeout!");
        force_send_control_ = false;
        if (hvac_settings_.valid)
          hvac_settings_.reset();
        forced_request_status_ = true;
        forced_publish_ = true;
        set_phase(ProtocolPhases::IDLE);
      }
      else if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_timestamp_).count() > CONTROL_MESSAGES_INTERVAL_MS)) // Usiing CONTROL_MESSAGES_INTERVAL_MS to speeduprequests
      {
        HaierProtocol::HaierMessage message = get_control_message();
        send_message(message);
        set_phase(ProtocolPhases::WAITING_CONTROL_ANSWER);       
      }
      break;
    case ProtocolPhases::WAITING_ANSWER_INIT_1:
    case ProtocolPhases::WAITING_ANSWER_INIT_2:
    case ProtocolPhases::WAITING_FIRST_STATUS_ANSWER:
    case ProtocolPhases::WAITING_ALARM_STATUS_ANSWER:
    case ProtocolPhases::WAITING_STATUS_ANSWER:
    case ProtocolPhases::WAITING_UPDATE_SIGNAL_ANSWER:
    case ProtocolPhases::WAITING_SIGNAL_LEVEL_ANSWER:
    case ProtocolPhases::WAITING_CONTROL_ANSWER:
      break;
    case ProtocolPhases::IDLE:
      {
        if (forced_request_status_ || (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_status_request_).count() > STATUS_REQUEST_INTERVAL_MS))
        {
          set_phase(ProtocolPhases::SENDING_STATUS_REQUEST);
          forced_request_status_ = false;
        }
        else if (send_wifi_signal_ && (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_signal_request_).count() > SIGNAL_LEVEL_UPDATE_INTERVAL_MS))
          set_phase(ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST);
      }
      break;
    default:
      // Shouldn't get here
      ESP_LOGE(TAG, "Wrong protocol handler state: %d, resetting communication", protocol_phase_);
      set_phase(ProtocolPhases::SENDING_INIT_1);
      break;
  }
  this->haier_protocol_.loop();
}

ClimateTraits HaierClimate::traits()
{
  return traits_;
}

void HaierClimate::control(const ClimateCall &call)
{
  ESP_LOGD("Control", "Control call");
  if (protocol_phase_ < ProtocolPhases::IDLE)
  {
    ESP_LOGW(TAG, "Can't send control packet, first poll answer not received");
    return; //cancel the control, we cant do it without a poll answer.
  }
  if (hvac_settings_.valid)
  {
    ESP_LOGW(TAG, "Overriding old valid settings before they were applied!");
  }
  {
    if (call.get_mode().has_value())
      hvac_settings_.mode = call.get_mode();
    if (call.get_fan_mode().has_value())
      hvac_settings_.fan_mode = call.get_fan_mode();
    if (call.get_swing_mode().has_value())
      hvac_settings_.swing_mode =  call.get_swing_mode();
    if (call.get_target_temperature().has_value())
      hvac_settings_.target_temperature = call.get_target_temperature();
    if (call.get_preset().has_value())
      hvac_settings_.preset = call.get_preset();
    hvac_settings_.valid = true;
  }
  control_called_ = true;
}
const HaierProtocol::HaierMessage HaierClimate::get_control_message()
{
  uint8_t controlOutBuffer[sizeof(hon::HaierPacketControl)];
  memcpy(controlOutBuffer, last_status_message_, sizeof(hon::HaierPacketControl));
  ESP_LOGV(TAG, "Last Status Message => %s", buf2hex(last_status_message_, sizeof(hon::HaierPacketControl)).c_str());
  hon::HaierPacketControl* outData = (hon::HaierPacketControl*)controlOutBuffer;
  bool hasHvacSettings = false;
  if (hvac_settings_.valid)
  {
    hasHvacSettings = true;
    HvacSettings climateControl;
    climateControl = hvac_settings_;
    if (climateControl.mode.has_value())
    {
      switch (climateControl.mode.value())
      {
      case CLIMATE_MODE_OFF:
        outData->ac_power = 0;
        break;

      case CLIMATE_MODE_AUTO:
        outData->ac_power = 1;
        outData->ac_mode = (uint8_t)hon::ConditioningMode::AUTO;
        outData->fan_mode = other_modes_fan_speed_;
        break;

      case CLIMATE_MODE_HEAT:
        outData->ac_power = 1;
        outData->ac_mode = (uint8_t)hon::ConditioningMode::HEAT;
        outData->fan_mode = other_modes_fan_speed_;
        break;

      case CLIMATE_MODE_DRY:
        outData->ac_power = 1;
        outData->ac_mode = (uint8_t)hon::ConditioningMode::DRY;
        outData->fan_mode = other_modes_fan_speed_;
        break;

      case CLIMATE_MODE_FAN_ONLY:
        outData->ac_power = 1;
        outData->ac_mode = (uint8_t)hon::ConditioningMode::FAN;
        outData->fan_mode = fan_mode_speed_;    // Auto doesn't work in fan only mode
        break;

      case CLIMATE_MODE_COOL:
        outData->ac_power = 1;
        outData->ac_mode = (uint8_t)hon::ConditioningMode::COOL;
        outData->fan_mode = other_modes_fan_speed_;
        break;
      default:
        ESP_LOGE("Control", "Unsupported climate mode");
        break;
      }
    }
    //Set fan speed, if we are in fan mode, reject auto in fan mode
    if (climateControl.fan_mode.has_value())
    {
      switch (climateControl.fan_mode.value())
      {
      case CLIMATE_FAN_LOW:
        outData->fan_mode = (uint8_t)hon::FanMode::LOW;
        break;
      case CLIMATE_FAN_MEDIUM:
        outData->fan_mode = (uint8_t)hon::FanMode::MID;
        break;
      case CLIMATE_FAN_HIGH:
        outData->fan_mode = (uint8_t)hon::FanMode::HIGH;
        break;
      case CLIMATE_FAN_AUTO:
        if (mode != CLIMATE_MODE_FAN_ONLY) //if we are not in fan only mode
          outData->fan_mode = (uint8_t)hon::FanMode::AUTO;
        break;
      default:
        ESP_LOGE("Control", "Unsupported fan mode");
        break;
      }
    }
    //Set swing mode
    if (climateControl.swing_mode.has_value())
    {
      switch (climateControl.swing_mode.value())
      {
      case CLIMATE_SWING_OFF:
        outData->horizontal_swing_mode = (uint8_t)get_horizontal_swing_mode(horizontal_direction_);
        outData->vertical_swing_mode = (uint8_t)get_vertical_swing_mode(vertical_direction_);
        break;
      case CLIMATE_SWING_VERTICAL:
        outData->horizontal_swing_mode = (uint8_t)get_horizontal_swing_mode(horizontal_direction_);
        outData->vertical_swing_mode = (uint8_t)hon::VerticalSwingMode::AUTO;
        break;
      case CLIMATE_SWING_HORIZONTAL:
        outData->horizontal_swing_mode = (uint8_t)hon::HorizontalSwingMode::AUTO;
        outData->vertical_swing_mode = (uint8_t)get_vertical_swing_mode(vertical_direction_);
        break;
      case CLIMATE_SWING_BOTH:
        outData->horizontal_swing_mode = (uint8_t)hon::HorizontalSwingMode::AUTO;
        outData->vertical_swing_mode = (uint8_t)hon::VerticalSwingMode::AUTO;
        break;
      }
    }
    if (climateControl.target_temperature.has_value())
      outData->set_point = climateControl.target_temperature.value() - 16; //set the temperature at our offset, subtract 16.
    if (climateControl.preset.has_value())
    {
      switch (climateControl.preset.value())
      {
      case CLIMATE_PRESET_NONE:
        outData->quiet_mode = 0;
        outData->fast_mode = 0;
        outData->sleep_mode = 0;
        break;
      case CLIMATE_PRESET_ECO:
        outData->quiet_mode = 1;
        outData->fast_mode = 0;
        outData->sleep_mode = 0;
        break;
      case CLIMATE_PRESET_BOOST:
        outData->quiet_mode = 0;
        outData->fast_mode = 1;
        outData->sleep_mode = 0;
        break;
      case CLIMATE_PRESET_AWAY:
        outData->quiet_mode = 0;
        outData->fast_mode = 0;
        outData->sleep_mode = 0;
        break;
      case CLIMATE_PRESET_SLEEP:
        outData->quiet_mode = 0;
        outData->fast_mode = 0;
        outData->sleep_mode = 1;
        break;
      default:
        ESP_LOGE("Control", "Unsupported preset");
        break;
      }
    }
  }
  else
  {
    if (outData->vertical_swing_mode != (uint8_t)hon::VerticalSwingMode::AUTO)
      outData->vertical_swing_mode = (uint8_t)get_vertical_swing_mode(vertical_direction_);
    if (outData->horizontal_swing_mode != (uint8_t)hon::HorizontalSwingMode::AUTO)
      outData->horizontal_swing_mode = (uint8_t)get_horizontal_swing_mode(horizontal_direction_);
  }
  outData->beeper_status = ((!beeper_status_) || (!hasHvacSettings)) ? 1 : 0;
  controlOutBuffer[4] = 0;   // This byte should be cleared before setting values
  outData->display_status = display_status_ ? 1 : 0;
  const HaierProtocol::HaierMessage control_message((uint8_t)hon::FrameType::CONTROL, (uint16_t)hon::SubcomandsControl::SET_GROUP_PARAMETERS, controlOutBuffer, sizeof(hon::HaierPacketControl));
  return control_message;
}

HaierProtocol::HandlerError HaierClimate::process_status_message(const uint8_t* packetBuffer, uint8_t size)
{
  if (size < sizeof(hon::HaierStatus))
    return HaierProtocol::HandlerError::heWrongMessageStructure;
  hon::HaierStatus packet;
  if (size < sizeof(hon::HaierStatus))
    size = sizeof(hon::HaierStatus);
  memcpy(&packet, packetBuffer, size);
  if (packet.sensors.error_status != 0)
  {
    ESP_LOGW(TAG, "HVAC error, code=0x%02X message: %s",
      packet.sensors.error_status,
      (packet.sensors.error_status < sizeof(hon::ErrorMessages)) ? hon::ErrorMessages[packet.sensors.error_status].c_str() : "Unknown error");
  }
  if (outdoor_sensor_ != nullptr)
  {
    float otemp = (float)(packet.sensors.outdoor_temperature + PROTOCOL_OUTDOOR_TEMPERATURE_OFFSET);
    if ((!outdoor_sensor_->has_state()) || (outdoor_sensor_->get_raw_state() != otemp))
      outdoor_sensor_->publish_state(otemp);
  }
  bool shouldPublish = false;    
  {
    // Extra modes/presets
    optional<ClimatePreset> oldPreset = preset;
    if (packet.control.quiet_mode != 0)
    {
      preset = CLIMATE_PRESET_ECO;
    }
    else if (packet.control.fast_mode != 0)
    {
      preset = CLIMATE_PRESET_BOOST;
    }
    else if (packet.control.sleep_mode != 0)
    {
      preset = CLIMATE_PRESET_SLEEP;
    }
    else
    {
      preset = CLIMATE_PRESET_NONE;
    }
    shouldPublish = shouldPublish || (!oldPreset.has_value()) || (oldPreset.value() != preset.value());
  }
  {
    // Target temperature
    float oldTargetTemperature = target_temperature;
    target_temperature = packet.control.set_point + 16.0f;
    shouldPublish = shouldPublish || (oldTargetTemperature != target_temperature);
  }
  {
    // Current temperature
    float oldCurrentTemperature = current_temperature;
    current_temperature = packet.sensors.room_temperature / 2.0f;
    shouldPublish = shouldPublish || (oldCurrentTemperature != current_temperature);
  }
  {
    // Fan mode
    optional<ClimateFanMode> oldFanMode = fan_mode;
    //remember the fan speed we last had for climate vs fan
    if (packet.control.ac_mode == (uint8_t)hon::ConditioningMode::FAN)
      fan_mode_speed_ = packet.control.fan_mode;
    else
      other_modes_fan_speed_ = packet.control.fan_mode;
    switch (packet.control.fan_mode)
    {
    case (uint8_t)hon::FanMode::AUTO:
      fan_mode = CLIMATE_FAN_AUTO;
      break;
    case (uint8_t)hon::FanMode::MID:
      fan_mode = CLIMATE_FAN_MEDIUM;
      break;
    case (uint8_t)hon::FanMode::LOW:
      fan_mode = CLIMATE_FAN_LOW;
      break;
    case (uint8_t)hon::FanMode::HIGH:
      fan_mode = CLIMATE_FAN_HIGH;
      break;
    }
    shouldPublish = shouldPublish || (!oldFanMode.has_value()) || (oldFanMode.value() != fan_mode.value());
  }
  {
    // Climate mode
    ClimateMode oldMode = mode;
    if (packet.control.ac_power == 0)
      mode = CLIMATE_MODE_OFF;
    else
    {
      // Check current hvac mode
      switch (packet.control.ac_mode)
      {
      case (uint8_t)hon::ConditioningMode::COOL:
        mode = CLIMATE_MODE_COOL;
        break;
      case (uint8_t)hon::ConditioningMode::HEAT:
        mode = CLIMATE_MODE_HEAT;
        break;
      case (uint8_t)hon::ConditioningMode::DRY:
        mode = CLIMATE_MODE_DRY;
        break;
      case (uint8_t)hon::ConditioningMode::FAN:
        mode = CLIMATE_MODE_FAN_ONLY;
        break;
      case (uint8_t)hon::ConditioningMode::AUTO:
        mode = CLIMATE_MODE_AUTO;
        break;
      }
    }
    shouldPublish = shouldPublish || (oldMode != mode);
  }
  {
    // Swing mode
    ClimateSwingMode oldSwingMode = swing_mode;
    if (packet.control.horizontal_swing_mode == (uint8_t)hon::HorizontalSwingMode::AUTO)
    {
      if (packet.control.vertical_swing_mode == (uint8_t)hon::VerticalSwingMode::AUTO)
        swing_mode = CLIMATE_SWING_BOTH;
      else
        swing_mode = CLIMATE_SWING_HORIZONTAL;
    }
    else
    {
      if (packet.control.vertical_swing_mode == (uint8_t)hon::VerticalSwingMode::AUTO)
        swing_mode = CLIMATE_SWING_VERTICAL;
      else
        swing_mode = CLIMATE_SWING_OFF;
    }
    shouldPublish = shouldPublish || (oldSwingMode != swing_mode);
  }
  last_valid_status_timestamp_ = std::chrono::steady_clock::now();
  if (forced_publish_ || shouldPublish)
  {
#if (HAIER_LOG_LEVEL > 4)
    auto _publish_start = std::chrono::high_resolution_clock::now();
#endif
    this->publish_state();
#if (HAIER_LOG_LEVEL > 4)
    ESP_LOGV(TAG, "Publish delay: %d ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - _publish_start).count());
#endif
    forced_publish_ = false;
  }
  if (shouldPublish)
  {
    ESP_LOGI(TAG, "HVAC values changed");
  }
  esp_log_printf_((shouldPublish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__, "HVAC Mode = 0x%X", packet.control.ac_mode);
  esp_log_printf_((shouldPublish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__, "Fan speed Status = 0x%X", packet.control.fan_mode);
  esp_log_printf_((shouldPublish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__, "Horizontal Swing Status = 0x%X", packet.control.horizontal_swing_mode);
  esp_log_printf_((shouldPublish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__, "Vertical Swing Status = 0x%X", packet.control.vertical_swing_mode);
  esp_log_printf_((shouldPublish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__, "Set Point Status = 0x%X", packet.control.set_point);
  return HaierProtocol::HandlerError::heOK;
}

void HaierClimate::send_message(const HaierProtocol::HaierMessage& command)
{
  this->haier_protocol_.sendMessage(command, use_crc_);
  last_request_timestamp_ = std::chrono::steady_clock::now();;
}

void HaierClimate::HvacSettings::reset()
{
  valid = false;
  mode.reset();
  fan_mode.reset();
  swing_mode.reset();
  target_temperature.reset();
  preset.reset();
}

} // namespace haier
} // namespace esphome
