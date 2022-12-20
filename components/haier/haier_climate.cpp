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

VerticalSwingMode get_vertical_swing_mode(AirflowVerticalDirection direction)
{
  switch (direction)
  {
    case AirflowVerticalDirection::UP:
      return VerticalSwingUp;
    case AirflowVerticalDirection::DOWN:
      return VerticalSwingDown;
    default:
      return VerticalSwingCenter;
  }
}

HorizontalSwingMode get_horizontal_swing_mode(AirflowHorizontalDirection direction)
{
  switch (direction)
  {
    case AirflowHorizontalDirection::LEFT:
      return HorizontalSwingLeft;
    case AirflowHorizontalDirection::RIGHT:
      return HorizontalSwingRight;
    default:
      return HorizontalSwingCenter;
  }
}

HaierClimate::HaierClimate(UARTComponent* parent) :
              Component(),
              UARTDevice(parent),
              mHaierProtocol(*this),
              mPhase(ProtocolPhases::SENDING_INIT_1),
              mFanModeFanSpeed(FanMid),
              mOtherModesFanSpeed(FanAuto),
              mSendWifiSignal(false),
              mBeeperStatus(true),
              mDisplayStatus(true),
              mForceSendControl(false),
              mForcedPublish(false),
              mForcedRequestStatus(false),
              mControlCalled(false),
              mHvacHardwareInfoAvailable(false),
              mHvacFunctions{false, false, false, false, false},
              mUseCrc(mHvacFunctions[2]),
              mActiveAlarms{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
              mOutdoorSensor(nullptr)
{
  mLastStatusMessage = new uint8_t[sizeof(HaierPacketControl)];
  mTraits = climate::ClimateTraits();
  mTraits.set_supported_modes(
  {
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_FAN_ONLY,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_AUTO
  });
  mTraits.set_supported_fan_modes(
  {
    climate::CLIMATE_FAN_AUTO,
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH,
  });
  mTraits.set_supported_swing_modes(
  {
    climate::CLIMATE_SWING_OFF,
    climate::CLIMATE_SWING_BOTH,
    climate::CLIMATE_SWING_VERTICAL,
    climate::CLIMATE_SWING_HORIZONTAL
  });
  mTraits.set_supported_presets(
  {
    climate::CLIMATE_PRESET_NONE,
    climate::CLIMATE_PRESET_ECO,
    climate::CLIMATE_PRESET_BOOST,
    climate::CLIMATE_PRESET_SLEEP,
  });
  mTraits.set_supports_current_temperature(true);
  mVerticalDirection = AirflowVerticalDirection::CENTER;
  mHorizontalDirection = AirflowHorizontalDirection::CENTER;
  //mLastValidStatusTimestamp = std::chrono::steady_clock::now();
}

HaierClimate::~HaierClimate()
{
  delete[] mLastStatusMessage;
}

void HaierClimate::set_phase(ProtocolPhases phase)
{
  if (mPhase != phase)
  {
    ESP_LOGV(TAG, "Phase transition: %d => %d", mPhase, phase);
    mPhase = phase;
  }
}

void HaierClimate::set_send_wifi_signal(bool sendWifi) 
{
  mSendWifiSignal = sendWifi; 
}

void HaierClimate::set_beeper_state(bool state)
{
  mBeeperStatus = state;
}

bool HaierClimate::get_beeper_state() const
{
  return mBeeperStatus;
}

bool HaierClimate::get_display_state() const
{
  return mDisplayStatus;
}

void HaierClimate::set_display_state(bool state)
{
  if (mDisplayStatus != state)
  {
    mDisplayStatus = state;
    mForceSendControl = true;
  }
}

void HaierClimate::set_outdoor_temperature_sensor(esphome::sensor::Sensor *sensor) 
{
  mOutdoorSensor = sensor; 
}

AirflowVerticalDirection HaierClimate::get_vertical_airflow() const
{
  return mVerticalDirection;
};

void HaierClimate::set_vertical_airflow(AirflowVerticalDirection direction)
{
  if (direction > AirflowVerticalDirection::DOWN)
    mVerticalDirection = AirflowVerticalDirection::CENTER;
  else
    mVerticalDirection = direction;
  mForceSendControl = true;
}

AirflowHorizontalDirection HaierClimate::get_horizontal_airflow() const
{
  return mHorizontalDirection;
}

void HaierClimate::set_horizontal_airflow(AirflowHorizontalDirection direction)
{
  if (direction > AirflowHorizontalDirection::RIGHT)
    mHorizontalDirection = AirflowHorizontalDirection::CENTER;
  else
    mHorizontalDirection = direction;
  mForceSendControl = true;
}

void HaierClimate::set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes)
{
  mTraits.set_supported_swing_modes(modes);
  mTraits.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);   // Always available
  mTraits.add_supported_swing_mode(CLIMATE_SWING_VERTICAL);       // Always available
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
  if ((expectedRequestMessageType != (uint8_t)FrameType::NO_COMMAND) && (requestMessageType != expectedRequestMessageType))
    result = HaierProtocol::HandlerError::heUnsuportedMessage;
  if ((expectedAnswerMessageType != (uint8_t)FrameType::NO_COMMAND) && (answerMessageType != expectedAnswerMessageType))
    result = HaierProtocol::HandlerError::heUnsuportedMessage;
  if ((expectedPhase != ProtocolPhases::UNKNOWN) && (expectedPhase != mPhase))
    result = HaierProtocol::HandlerError::heUnexpectedMessage;
  if (answerMessageType == (uint8_t)FrameType::INVALID)
    result = HaierProtocol::HandlerError::heInvalidAnswer;
  return result;
}

HaierProtocol::HandlerError HaierClimate::get_device_version_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  HaierProtocol::HandlerError result = answer_preprocess(requestType, (uint8_t)FrameType::GET_DEVICE_VERSION, messageType, (uint8_t)FrameType::GET_DEVICE_VERSION_RESPONSE, ProtocolPhases::WAITING_ANSWER_INIT_1);
  if (result == HaierProtocol::HandlerError::heOK)
  {
    if (dataSize < sizeof(DeviceVersionAnswer))
    {
      // Wrong structure
      set_phase(ProtocolPhases::SENDING_INIT_1);
      return HaierProtocol::HandlerError::heWrongMessageStructure;
    }
    // All OK
    DeviceVersionAnswer* answr = (DeviceVersionAnswer*)data;
    char tmp[9];
    tmp[8] = 0;
    strncpy(tmp, answr->protocol_version, 8);
    mHvacProtocolVersion = std::string(tmp);
    strncpy(tmp, answr->software_version, 8);
    mHvacSoftwareVersion = std::string(tmp);
    strncpy(tmp, answr->hardware_version, 8);
    mHvacHardwareVersion = std::string(tmp);
    strncpy(tmp, answr->device_name, 8);
    mHvacDeviceName = std::string(tmp);
    mHvacFunctions[0] = (answr->functions[1] & 0x01) != 0;      // interactive mode support
    mHvacFunctions[1] = (answr->functions[1] & 0x02) != 0;      // master-slave mode support
    mHvacFunctions[2] = (answr->functions[1] & 0x04) != 0;      // crc support
    mHvacFunctions[3] = (answr->functions[1] & 0x08) != 0;      // multiple AC support
    mHvacFunctions[4] = (answr->functions[1] & 0x20) != 0;      // roles support
    mHvacHardwareInfoAvailable = true;
    set_phase(ProtocolPhases::SENDING_INIT_2);
    return result;
  }
  else
  {
    set_phase((mPhase >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE : ProtocolPhases::SENDING_INIT_1);
    return result;
  }
}

HaierProtocol::HandlerError HaierClimate::get_device_id_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  HaierProtocol::HandlerError result = answer_preprocess(requestType, (uint8_t)FrameType::GET_DEVICE_ID, messageType, (uint8_t)FrameType::GET_DEVICE_ID_RESPONSE, ProtocolPhases::WAITING_ANSWER_INIT_2);
  if (result == HaierProtocol::HandlerError::heOK)
  {
    set_phase(ProtocolPhases::SENDING_FIRST_STATUS_REQUEST);
    return result;
  }
  else
  {
    set_phase((mPhase >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE : ProtocolPhases::SENDING_INIT_1);
    return result;
  }
}

HaierProtocol::HandlerError HaierClimate::status_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  HaierProtocol::HandlerError result = answer_preprocess(requestType, (uint8_t)FrameType::CONTROL, messageType, (uint8_t)FrameType::STATUS, ProtocolPhases::UNKNOWN);
  if (result == HaierProtocol::HandlerError::heOK)
  {
    result = process_status_message(data, dataSize);
    if (result != HaierProtocol::HandlerError::heOK)
    {
      ESP_LOGW(TAG, "Error %d while parsing Status packet", (int)result);
      set_phase((mPhase >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE : ProtocolPhases::SENDING_INIT_1);
    }
    else
    {
      if (dataSize >= sizeof(HaierPacketControl) + 2)
      {
        memcpy(mLastStatusMessage, data + 2, sizeof(HaierPacketControl));
        ESP_LOGV(TAG, "Last status message <= %s", buf2hex(mLastStatusMessage, sizeof(HaierPacketControl)).c_str());
      }
      else
        ESP_LOGW(TAG, "Status packet too small: %d (should be >= %d)", dataSize, sizeof(HaierPacketControl));
      if (mPhase == ProtocolPhases::WAITING_FIRST_STATUS_ANSWER)
      {
        ESP_LOGI(TAG, "First HVAC status received");
        set_phase(ProtocolPhases::SENDING_ALARM_STATUS_REQUEST);
      }
      else if (mPhase == ProtocolPhases::WAITING_STATUS_ANSWER)
        set_phase(ProtocolPhases::IDLE);
      else if (mPhase == ProtocolPhases::WAITING_CONTROL_ANSWER)
      {
        set_phase(ProtocolPhases::IDLE); 
        mForceSendControl = false;
        if (mHvacSettings.valid)
          mHvacSettings.reset();
      }
    }
    return result;
  }
  else
  {
    set_phase((mPhase >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE : ProtocolPhases::SENDING_INIT_1);
    return result;
  }
}

HaierProtocol::HandlerError HaierClimate::get_management_information_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  HaierProtocol::HandlerError result = answer_preprocess(requestType, (uint8_t)FrameType::GET_MANAGEMENT_INFORMATION, messageType, (uint8_t)FrameType::GET_MANAGEMENT_INFORMATION_RESPONSE, ProtocolPhases::WAITING_UPDATE_SIGNAL_ANSWER);
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
  HaierProtocol::HandlerError result = answer_preprocess(requestType, (uint8_t)FrameType::REPORT_NETWORK_STATUS, messageType, (uint8_t)FrameType::CONFIRM, ProtocolPhases::WAITING_SIGNAL_LEVEL_ANSWER);
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
  if (requestType == (uint8_t)FrameType::GET_ALARM_STATUS)
  {
    if (messageType != (uint8_t)FrameType::GET_ALARM_STATUS_RESPONSE)
    {
      // Unexpected answer to request
      set_phase(ProtocolPhases::IDLE);
      return HaierProtocol::HandlerError::heUnsuportedMessage;
    }
    if (mPhase != ProtocolPhases::WAITING_ALARM_STATUS_ANSWER)
    {
      // Don't expect this answer now
      set_phase(ProtocolPhases::IDLE);
      return HaierProtocol::HandlerError::heUnexpectedMessage;
    }
    memcpy(mActiveAlarms, data + 2, 8);
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
  ESP_LOGW(TAG, "Answer timeout for command %02X, phase %d", requestType, mPhase);
  if (mPhase > ProtocolPhases::IDLE)
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
  mLastRequestTimestamp = std::chrono::steady_clock::now();
  set_phase(ProtocolPhases::SENDING_INIT_1);
  // Set handlers
#define SET_HANDLER(command, handler) mHaierProtocol.setAnswerHandler((uint8_t)(command), std::bind(&handler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  SET_HANDLER(FrameType::GET_DEVICE_VERSION, esphome::haier::HaierClimate::get_device_version_answer_handler);
  SET_HANDLER(FrameType::GET_DEVICE_ID, esphome::haier::HaierClimate::get_device_id_answer_handler);
  SET_HANDLER(FrameType::CONTROL, esphome::haier::HaierClimate::status_handler);
  SET_HANDLER(FrameType::GET_MANAGEMENT_INFORMATION, esphome::haier::HaierClimate::get_management_information_answer_handler);
  SET_HANDLER(FrameType::GET_ALARM_STATUS, esphome::haier::HaierClimate::get_alarm_status_answer_handler);
  SET_HANDLER(FrameType::REPORT_NETWORK_STATUS, esphome::haier::HaierClimate::report_network_status_answer_handler);
#undef SET_HANDLER
  mHaierProtocol.setDefaultTimeoutHandler(std::bind(&esphome::haier::HaierClimate::timeout_default_handler, this, std::placeholders::_1));
}

void HaierClimate::dump_config()
{
  LOG_CLIMATE("", "Haier hOn Climate", this);
  if (mHvacHardwareInfoAvailable)
  {
    ESP_LOGCONFIG(TAG, "  Device protocol version: %s",
      mHvacProtocolVersion.c_str());
    ESP_LOGCONFIG(TAG, "  Device software version: %s", 
      mHvacSoftwareVersion.c_str());
    ESP_LOGCONFIG(TAG, "  Device hardware version: %s",
      mHvacHardwareVersion.c_str());
    ESP_LOGCONFIG(TAG, "  Device name: %s", 
      mHvacDeviceName.c_str());
    ESP_LOGCONFIG(TAG, "  Device features:%s%s%s%s%s",
      (mHvacFunctions[0] ? " interactive" : ""), 
      (mHvacFunctions[1] ? " master-slave" : ""),
      (mHvacFunctions[2] ? " crc" : ""), 
      (mHvacFunctions[3] ? " multinode" : ""),
      (mHvacFunctions[4] ? " role" : ""));
    ESP_LOGCONFIG(TAG, "  Active alarms: %s", 
      buf2hex(mActiveAlarms, sizeof(mActiveAlarms)).c_str());
#if (HAIER_LOG_LEVEL > 4)
    ESP_LOGCONFIG(TAG, "  Protocol phase: %d",
      mPhase);
#endif
  }
}

void HaierClimate::loop()
{
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastValidStatusTimestamp).count() > COMMUNICATION_TIMEOUT_MS)
  {
    if (mPhase > ProtocolPhases::IDLE)
    {
      // No status too long, reseting protocol
      ESP_LOGW(TAG, "Communication timeout, reseting protocol");
      mLastValidStatusTimestamp = now;
      mForceSendControl = false;
      if (mHvacSettings.valid)
        mHvacSettings.reset();
      set_phase(ProtocolPhases::SENDING_INIT_1);
      return;
    }
    else
      // No need to reset protocol if we didn't pass initialization phase
      mLastValidStatusTimestamp = now;
  };
  if (mHvacSettings.valid || mForceSendControl)
  {
    // If control message is pending we should send it ASAP unless we are in initialisation procedure or waiting for an answer
    if (    (mPhase == ProtocolPhases::IDLE) ||
        (mPhase == ProtocolPhases::SENDING_STATUS_REQUEST) || 
        (mPhase == ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST) ||
        (mPhase == ProtocolPhases::SENDING_SIGNAL_LEVEL))
    {
      ESP_LOGV(TAG, "Control packet is pending...");
      set_phase(ProtocolPhases::SENDING_CONTROL);
    }
  }
  switch (mPhase)
  {
    case ProtocolPhases::SENDING_INIT_1:
      if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastRequestTimestamp).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        mHvacHardwareInfoAvailable = false;
        // Indicate device capabilities:
        // bit 0 - if 1 module support interactive mode
        // bit 1 - if 1 module support master-slave mode
        // bit 2 - if 1 module support crc
        // bit 3 - if 1 module support multiple devices
        // bit 4..bit 15 - not used
        uint8_t module_capabilities[2] = { 0b00000000, 0b00000111 };     
        static const HaierProtocol::HaierMessage deviceVersion_request((uint8_t)FrameType::GET_DEVICE_VERSION, module_capabilities, sizeof(module_capabilities));
        send_message(deviceVersion_request);
        set_phase(ProtocolPhases::WAITING_ANSWER_INIT_1);
      }
      break;
    case ProtocolPhases::SENDING_INIT_2:
      if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastRequestTimestamp).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static const HaierProtocol::HaierMessage deviceId_request((uint8_t)FrameType::GET_DEVICE_ID);
        send_message(deviceId_request);
        set_phase(ProtocolPhases::WAITING_ANSWER_INIT_2);
      }
      break;
    case ProtocolPhases::SENDING_FIRST_STATUS_REQUEST:
    case ProtocolPhases::SENDING_STATUS_REQUEST:
      if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastRequestTimestamp).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static const HaierProtocol::HaierMessage status_request((uint8_t)FrameType::CONTROL, (uint16_t)SubcomandsControl::CONTROL_GET_USER_DATA);
        send_message(status_request);
        mLastStatusRequest = now;
        set_phase((ProtocolPhases)((uint8_t)mPhase + 1));
      }
      break;
    case ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST:
      if (!mSendWifiSignal)
        set_phase(ProtocolPhases::IDLE);
      else if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastRequestTimestamp).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static const HaierProtocol::HaierMessage update_signal_request((uint8_t)FrameType::GET_MANAGEMENT_INFORMATION);
        send_message(update_signal_request);
        mLastSignalRequest = now;
        set_phase(ProtocolPhases::WAITING_UPDATE_SIGNAL_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_SIGNAL_LEVEL:
      if (!mSendWifiSignal)
        set_phase(ProtocolPhases::IDLE);
      else if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastRequestTimestamp).count() > DEFAULT_MESSAGES_INTERVAL_MS))
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
        HaierProtocol::HaierMessage wifi_status_request((uint8_t)FrameType::REPORT_NETWORK_STATUS, wifi_status_data, sizeof(wifi_status_data));
        send_message(wifi_status_request);
        set_phase(ProtocolPhases::WAITING_SIGNAL_LEVEL_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_ALARM_STATUS_REQUEST:
      if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastRequestTimestamp).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static const HaierProtocol::HaierMessage alarm_status_request((uint8_t)FrameType::GET_ALARM_STATUS);
        send_message(alarm_status_request);
        set_phase(ProtocolPhases::WAITING_ALARM_STATUS_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_CONTROL:
      if (mControlCalled)
      {
        mControlRequestTimestamp = now;
        mControlCalled = false;
      }
      if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mControlRequestTimestamp).count() > CONTROL_TIMEOUT_MS)
      {
        ESP_LOGW(TAG, "Sending control packet timeout!");
        mForceSendControl = false;
        if (mHvacSettings.valid)
          mHvacSettings.reset();
        mForcedRequestStatus = true;
        mForcedPublish = true;
        set_phase(ProtocolPhases::IDLE);
      }
      else if (can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastRequestTimestamp).count() > CONTROL_MESSAGES_INTERVAL_MS)) // Usiing CONTROL_MESSAGES_INTERVAL_MS to speeduprequests
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
        if (mForcedRequestStatus || (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastStatusRequest).count() > STATUS_REQUEST_INTERVAL_MS))
        {
          set_phase(ProtocolPhases::SENDING_STATUS_REQUEST);
          mForcedRequestStatus = false;
        }
        else if (mSendWifiSignal && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastSignalRequest).count() > SIGNAL_LEVEL_UPDATE_INTERVAL_MS))
          set_phase(ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST);
      }
      break;
    default:
      // Shouldn't get here
      ESP_LOGE(TAG, "Wrong protocol handler state: %d, resetting communication", mPhase);
      set_phase(ProtocolPhases::SENDING_INIT_1);
      break;
  }
  mHaierProtocol.loop();
}

ClimateTraits HaierClimate::traits()
{
  return mTraits;
}

void HaierClimate::control(const ClimateCall &call)
{
  ESP_LOGD("Control", "Control call");
  if (mPhase < ProtocolPhases::IDLE)
  {
    ESP_LOGW(TAG, "Can't send control packet, first poll answer not received");
    return; //cancel the control, we cant do it without a poll answer.
  }
  if (mHvacSettings.valid)
  {
    ESP_LOGW(TAG, "Overriding old valid settings before they were applied!");
  }
  {
    if (call.get_mode().has_value())
      mHvacSettings.mode = call.get_mode();
    if (call.get_fan_mode().has_value())
      mHvacSettings.fan_mode = call.get_fan_mode();
    if (call.get_swing_mode().has_value())
      mHvacSettings.swing_mode =  call.get_swing_mode();
    if (call.get_target_temperature().has_value())
      mHvacSettings.target_temperature = call.get_target_temperature();
    if (call.get_preset().has_value())
      mHvacSettings.preset = call.get_preset();
    mHvacSettings.valid = true;
  }
  mControlCalled = true;
}
const HaierProtocol::HaierMessage HaierClimate::get_control_message()
{
  uint8_t controlOutBuffer[sizeof(HaierPacketControl)];
  memcpy(controlOutBuffer, mLastStatusMessage, sizeof(HaierPacketControl));
  ESP_LOGV(TAG, "Last Status Message => %s", buf2hex(mLastStatusMessage, sizeof(HaierPacketControl)).c_str());
  HaierPacketControl* outData = (HaierPacketControl*)controlOutBuffer;
  bool hasHvacSettings = false;
  if (mHvacSettings.valid)
  {
    hasHvacSettings = true;
    HvacSettings climateControl;
    climateControl = mHvacSettings;
    if (climateControl.mode.has_value())
    {
      switch (climateControl.mode.value())
      {
      case CLIMATE_MODE_OFF:
        outData->ac_power = 0;
        break;

      case CLIMATE_MODE_AUTO:
        outData->ac_power = 1;
        outData->ac_mode = ConditioningAuto;
        outData->fan_mode = mOtherModesFanSpeed;
        break;

      case CLIMATE_MODE_HEAT:
        outData->ac_power = 1;
        outData->ac_mode = ConditioningHeat;
        outData->fan_mode = mOtherModesFanSpeed;
        break;

      case CLIMATE_MODE_DRY:
        outData->ac_power = 1;
        outData->ac_mode = ConditioningDry;
        outData->fan_mode = mOtherModesFanSpeed;
        break;

      case CLIMATE_MODE_FAN_ONLY:
        outData->ac_power = 1;
        outData->ac_mode = ConditioningFan;
        outData->fan_mode = mFanModeFanSpeed;    // Auto doesn't work in fan only mode
        break;

      case CLIMATE_MODE_COOL:
        outData->ac_power = 1;
        outData->ac_mode = ConditioningCool;
        outData->fan_mode = mOtherModesFanSpeed;
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
        outData->fan_mode = FanLow;
        break;
      case CLIMATE_FAN_MEDIUM:
        outData->fan_mode = FanMid;
        break;
      case CLIMATE_FAN_HIGH:
        outData->fan_mode = FanHigh;
        break;
      case CLIMATE_FAN_AUTO:
        if (mode != CLIMATE_MODE_FAN_ONLY) //if we are not in fan only mode
          outData->fan_mode = FanAuto;
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
        outData->horizontal_swing_mode = get_horizontal_swing_mode(mHorizontalDirection);
        outData->vertical_swing_mode = get_vertical_swing_mode(mVerticalDirection);
        break;
      case CLIMATE_SWING_VERTICAL:
        outData->horizontal_swing_mode = get_horizontal_swing_mode(mHorizontalDirection);
        outData->vertical_swing_mode = VerticalSwingAuto;
        break;
      case CLIMATE_SWING_HORIZONTAL:
        outData->horizontal_swing_mode = HorizontalSwingAuto;
        outData->vertical_swing_mode = get_vertical_swing_mode(mVerticalDirection);
        break;
      case CLIMATE_SWING_BOTH:
        outData->horizontal_swing_mode = HorizontalSwingAuto;
        outData->vertical_swing_mode = VerticalSwingAuto;
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
    if (outData->vertical_swing_mode != VerticalSwingAuto)
      outData->vertical_swing_mode = get_vertical_swing_mode(mVerticalDirection);
    if (outData->horizontal_swing_mode != HorizontalSwingAuto)
      outData->horizontal_swing_mode = get_horizontal_swing_mode(mHorizontalDirection);
  }
  outData->beeper_status = ((!mBeeperStatus) || (!hasHvacSettings)) ? 1 : 0;
  controlOutBuffer[4] = 0;   // This byte should be cleared before setting values
  outData->display_status = mDisplayStatus ? 1 : 0;
  const HaierProtocol::HaierMessage control_message((uint8_t)FrameType::CONTROL, (uint16_t)SubcomandsControl::CONTROL_SET_GROUP_PARAMETERS, controlOutBuffer, sizeof(HaierPacketControl));
  return control_message;
}

HaierProtocol::HandlerError HaierClimate::process_status_message(const uint8_t* packetBuffer, uint8_t size)
{
  if (size < sizeof(HaierStatus))
    return HaierProtocol::HandlerError::heWrongMessageStructure;
  HaierStatus packet;
  if (size < sizeof(HaierStatus))
    size = sizeof(HaierStatus);
  memcpy(&packet, packetBuffer, size);

  if (packet.sensors.error_status != 0)
  {
    ESP_LOGW(TAG, "HVAC error, code=0x%02X message: %s",
      packet.sensors.error_status,
      (packet.sensors.error_status < sizeof(ErrorMessages)) ? ErrorMessages[packet.sensors.error_status].c_str() : "Unknown error");
  }
  if (mOutdoorSensor != nullptr)
  {
    float otemp = (float)(packet.sensors.outdoor_temperature + PROTOCOL_OUTDOOR_TEMPERATURE_OFFSET);
    if ((!mOutdoorSensor->has_state()) || (mOutdoorSensor->get_raw_state() != otemp))
      mOutdoorSensor->publish_state(otemp);
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
    if (packet.control.ac_mode == ConditioningFan)
      mFanModeFanSpeed = packet.control.fan_mode;
    else
      mOtherModesFanSpeed = packet.control.fan_mode;
    switch (packet.control.fan_mode)
    {
    case FanAuto:
      fan_mode = CLIMATE_FAN_AUTO;
      break;
    case FanMid:
      fan_mode = CLIMATE_FAN_MEDIUM;
      break;
    case FanLow:
      fan_mode = CLIMATE_FAN_LOW;
      break;
    case FanHigh:
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
      case ConditioningCool:
        mode = CLIMATE_MODE_COOL;
        break;
      case ConditioningHeat:
        mode = CLIMATE_MODE_HEAT;
        break;
      case ConditioningDry:
        mode = CLIMATE_MODE_DRY;
        break;
      case ConditioningFan:
        mode = CLIMATE_MODE_FAN_ONLY;
        break;
      case ConditioningAuto:
        mode = CLIMATE_MODE_AUTO;
        break;
      }
    }
    shouldPublish = shouldPublish || (oldMode != mode);
  }
  {
    // Swing mode
    ClimateSwingMode oldSwingMode = swing_mode;
    if (packet.control.horizontal_swing_mode == HorizontalSwingAuto)
    {
      if (packet.control.vertical_swing_mode == VerticalSwingAuto)
        swing_mode = CLIMATE_SWING_BOTH;
      else
        swing_mode = CLIMATE_SWING_HORIZONTAL;
    }
    else
    {
      if (packet.control.vertical_swing_mode == VerticalSwingAuto)
        swing_mode = CLIMATE_SWING_VERTICAL;
      else
        swing_mode = CLIMATE_SWING_OFF;
    }
    shouldPublish = shouldPublish || (oldSwingMode != swing_mode);
  }
  mLastValidStatusTimestamp = std::chrono::steady_clock::now();
  if (mForcedPublish || shouldPublish)
  {
#if (HAIER_LOG_LEVEL > 4)
    auto _publish_start = std::chrono::high_resolution_clock::now();
#endif
    this->publish_state();
#if (HAIER_LOG_LEVEL > 4)
    ESP_LOGV(TAG, "Publish delay: %d ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - _publish_start).count());
#endif
    mForcedPublish = false;
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
  mHaierProtocol.sendMessage(command, mUseCrc);
  mLastRequestTimestamp = std::chrono::steady_clock::now();;
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
