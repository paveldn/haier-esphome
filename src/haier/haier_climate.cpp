#include "esphome.h"
#include <chrono>
#include <mutex>
#include <string>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "haier_climate.h"
#include "haier_packet.h"

#define SEND_WIFI_SIGNAL

#ifdef SEND_WIFI_SIGNAL
#include "esphome/components/wifi/wifi_component.h"
#endif

using namespace esphome::climate;
using namespace esphome::uart;

namespace esphome {
namespace haier {

#define TAG "Haier"

#define PACKET_TIMOUT_MS                500
#define ANSWER_TIMOUT_MS                1000
#define COMMUNICATION_TIMOUT_MS         60000
#define STATUS_REQUEST_INTERVAL_MS      5000
#define SIGNAL_LEVEL_UPDATE_INTERVAL_MS 10000

// temperatures supported by AC system
#define MIN_SET_TEMPERATURE             16
#define MAX_SET_TEMPERATURE             30

#define MAX_MESSAGE_SIZE                64
#define HEADER                          0xFF

// ESP_LOG_LEVEL don't work as I want it so I implemented this macro
#define ESP_LOG_L(level, tag, format, ...) do {                     \
        if (level==ESPHOME_LOG_LEVEL_ERROR )        { ESP_LOGE(tag, format, __VA_ARGS__); } \
        else if (level==ESPHOME_LOG_LEVEL_WARN )    { ESP_LOGW(tag, format, __VA_ARGS__); } \
        else if (level==ESPHOME_LOG_LEVEL_INFO)     { ESP_LOGI(tag, format, __VA_ARGS__); } \
        else if (level==ESPHOME_LOG_LEVEL_DEBUG )   { ESP_LOGD(tag, format, __VA_ARGS__); } \
        else if (level==ESPHOME_LOG_LEVEL_VERBOSE ) { ESP_LOGV(tag, format, __VA_ARGS__); } \
        else                                        { ESP_LOGVV(tag, format, __VA_ARGS__); } \
    } while(0)

uint8_t getChecksum(const uint8_t * message, size_t size) {
        uint8_t result = 0;
        for (int i = 0; i < size; i++){
            result += message[i];
        }
        return result;
    }

unsigned short crc16(const uint8_t* message, int len, uint16_t initial_val = 0)
{
    constexpr uint16_t poly = 0xA001;
    uint16_t crc = initial_val;
    for(int i = 0; i < len; ++i)
    {
        crc ^= (unsigned short)message[i];
        for(int b = 0; b < 8; ++b)
        {
            if((crc & 1) != 0)
            {
                crc >>= 1;
                crc ^= poly;
            }
            else
                crc >>= 1;

        }
    }
    return crc;
}

std::string getHex(const uint8_t * message, size_t size)
{
    constexpr char hexmap[] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    std::string raw(size * 3, ' ');
    for (int i = 0; i < size; ++i) {
        raw[3*i]   = ' ';
        raw[3*i+1] = hexmap[(message[i] & 0xF0) >> 4];
        raw[3*i+2] = hexmap[message[i] & 0x0F];
    }
  return raw;
}

namespace
{
    enum HaierProtocolCommands
    {
        hpCommandInit1              = 0x61,
        hpCommandInit2              = 0x70,
        hpCommandStatus             = 0x01,
        hpCommandReadyToSendSignal  = 0xFC,
        hpCommandSignalLevel        = 0xF7,
        hpCommandControl            = 0x60,
    };

    enum HaierProtocolAnswers
    {
        hpAnswerInit1               = 0x62,
        hpAnswerInit2               = 0x71,
        hpAnswerRequestStatus       = 0x02,
        hpAnswerError               = 0x03,
        hpAnswerReadyToSendSignal   = 0xFD,
        hpAnswerControl             = 0x61,
    };
}

const HaierPacketHeader initialization_1 = {
        .msg_length = 0x0A,
        .reserved = { 0x00,   0x00,   0x00,   0x00,   0x00,   0x00 },
        .msg_type = (uint8_t)(hpCommandInit1),
        .arguments = { 0x00, 0x07 }
    };

const HaierPacketHeader initialization_2 = {
        .msg_length = 0x08,
        .reserved = { 0x40,   0x00,   0x00,   0x00,   0x00,   0x00 },
        .msg_type = (uint8_t)(hpCommandInit2),
        .arguments = { 0x00, 0x00 }
    };

const HaierPacketHeader poll_command = {
        .msg_length = 0x0A,
        .reserved = { 0x40,   0x00,   0x00,   0x00,   0x00,   0x00 },
        .msg_type = (uint8_t)(hpCommandStatus),
        // There is also longer version of this command when second
        // argument is 0xFE instead of 0x01. It gives more data but
        // my AC return all 0 there, probably it just not support
        // additional features
        .arguments = { 0x4D, 0x01 }
    };

const HaierPacketHeader wifi_request = {
        .msg_length = 0x08,
        .reserved = { 0x40,   0x00,   0x00,   0x00,   0x00,   0x00 },
        .msg_type = (uint8_t)(hpCommandReadyToSendSignal),
        .arguments = { 0x00, 0x00 }
    };

const HaierPacketHeader control_command = {
        .msg_length = CONTROL_PACKET_SIZE,
        .reserved = { 0x40,   0x00,   0x00,   0x00,   0x00,   0x00 },
        .msg_type = (uint8_t)(hpCommandStatus),
        .arguments = { 0x60, 0x01 }
    };

// This command is longer so we can't use HaierPacketHeader
 uint8_t wifi_status[]  = {0x0C,    0x40,   0x00,   0x00,   0x00,   0x00,   0x00,   (uint8_t)(hpCommandSignalLevel),    0x00,   0x00,   0x00,   0x00};

HaierClimate::HaierClimate(UARTComponent* parent) :
                                        Component(),
                                        UARTDevice(parent) ,
                                        mFanModeFanSpeed(FanMid),
                                        mOtherModesFanSpeed(FanAuto)
{
    mLastPacket = new uint8_t[MAX_MESSAGE_SIZE];
    mReadMutex = xSemaphoreCreateMutex();
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
        climate::CLIMATE_PRESET_AWAY

    });
    mTraits.set_visual_min_temperature(MIN_SET_TEMPERATURE);
    mTraits.set_visual_max_temperature(MAX_SET_TEMPERATURE);
    mTraits.set_visual_temperature_step(1.0f);
    mTraits.set_supports_current_temperature(true);
}

HaierClimate::~HaierClimate()
{
    vSemaphoreDelete(mReadMutex);
    delete[] mLastPacket;
}

void HaierClimate::setup()
{
    ESP_LOGI(TAG, "Haier initialization...");
    delay(2000); // Give time for AC to boot
    mPhase = psSendingInit1;
}

namespace   // anonymous namespace for local things
{
    struct CurrentPacketStatus
    {
        uint8_t buffer[MAX_MESSAGE_SIZE];
        uint8_t preHeaderCharsCounter;
        uint8_t size;
        uint8_t position;
        uint8_t checksum;
        CurrentPacketStatus() : preHeaderCharsCounter(0),
                                size(0),
                                position(0),
                                checksum(0)
        {};
        void Reset()
        {
            size = 0;
            position = 0;
            checksum = 0;
            preHeaderCharsCounter = 0;
        };
    };

    CurrentPacketStatus currentPacket;

}

void HaierClimate::loop()
{
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if ((mPhase >= psIdle) && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastValidStatusTimestamp).count() > COMMUNICATION_TIMOUT_MS))
    {
        ESP_LOGE(TAG, "No valid status answer for to long. Resetting protocol");
        currentPacket.Reset();
        mPhase = psSendingInit1;
        return;
    }
    switch (mPhase)
    {
        case psSendingInit1:
            // No CRC here. Old Haier protocol use just 1 byte checksum at the end.
            // Answer to this request should return version of protocol, so controller
            // can decide will we use CRC at the end or not based on version.
            // We are not supporting old protocol
            sendData((uint8_t*)&initialization_1, initialization_1.msg_length, false);
            mPhase = psWaitingAnswerInit1;
            mLastRequestTimestamp = now;
            return;
        case psSendingInit2:
            sendData((uint8_t*)&initialization_2, initialization_2.msg_length);
            mPhase = psWaitingAnswerInit2;
            mLastRequestTimestamp = now;
            return;
        case psSendingFirstStatusRequest:
        case psSendingStatusRequest:
            sendData((uint8_t*)&poll_command, poll_command.msg_length);
            mLastStatusRequest = now;
            mPhase = (ProtocolPhases)((uint8_t)mPhase + 1);
            mLastRequestTimestamp = now;
            return;
        case psSendingUpdateSignalRequest:
            sendData((uint8_t*)&wifi_request, wifi_request.msg_length);
            mLastSignalRequest = now;
            mPhase = psWaitingUpateSignalAnswer;
            mLastRequestTimestamp = now;
            return;
        case psSendingSignalLevel:
#ifdef SEND_WIFI_SIGNAL
            {
                if (wifi::global_wifi_component->is_connected())
                {
                    wifi_status[9] = 0;
                    int8_t _rssi = wifi::global_wifi_component->wifi_rssi();
                    wifi_status[11] = uint8_t((128 + _rssi) / 1.28f);
                    ESP_LOGD(TAG, "WiFi signal is: %ddBm => %d%%", _rssi, wifi_status[11]);
                }
                else
                {
                    ESP_LOGD(TAG, "WiFi is not connected");
                    wifi_status[9] = 1;
                    wifi_status[11] = 0;
                }
                sendData(wifi_status, wifi_status[0]);
            }
#endif
            mPhase = psIdle;    // We don't expect answer here
            return;
        case psWaitingAnswerInit1:
        case psWaitingAnswerInit2:
        case psWaitingFirstStatusAnswer:
            // Using status request interval here to avoid pushing to many messages if AC is not ready
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastRequestTimestamp).count() > STATUS_REQUEST_INTERVAL_MS)
            {
                // No valid communication yet, resetting protocol,
                // No logs to avoid to many messages
                currentPacket.Reset();
                mPhase = psSendingInit1;
                return;
            }
            break;
        case psWaitingStatusAnswer:
        case psWaitingUpateSignalAnswer:
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastRequestTimestamp).count() > ANSWER_TIMOUT_MS)
            {
                // We have valid communication here, no problem if we missed packet or two
                // Just request packet again in next loop call. We also protected by protocol timeout
                ESP_LOGW(TAG, "Request answer timeout, phase %d", mPhase);
                mPhase = psIdle;
                return;
            }
            break;
        case psIdle:
            // Just do nothing, fall through
            break;
        default:
            // Shouldn't get here
            ESP_LOGE(TAG, "Wrong protocol handler state: %d, resetting communication", mPhase);
            currentPacket.Reset();
            mPhase = psSendingInit1;
            return;
    }
    // Here we expect some input from AC or just waiting for the proper time to send the request
    // Anyway we read the port to make sure that the buffer does not overflow
    if ((currentPacket.size > 0) && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastByteTimestamp).count() > PACKET_TIMOUT_MS))
    {
        ESP_LOGW(TAG, "Incoming packet timeout, packet size %d, expected size %d", currentPacket.position, currentPacket.size);
        currentPacket.Reset();
    }
    getSerialData();
    if (mPhase == psIdle)
    {
        // If we not waiting for answers check if there is a proper time to request data
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastStatusRequest).count() > STATUS_REQUEST_INTERVAL_MS)
            mPhase = psSendingStatusRequest;
#ifdef SEND_WIFI_SIGNAL
        else if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastSignalRequest).count() > SIGNAL_LEVEL_UPDATE_INTERVAL_MS)
            mPhase = psSendingUpdateSignalRequest;
#endif
    }
}

void HaierClimate::getSerialData()
{
    while (available() > 0)
    {
        uint8_t val;
        if (!read_byte(&val))
            break;
        if (currentPacket.size > 0) // Already found packet start
        {
            if (currentPacket.position < currentPacket.size)
            {
                currentPacket.buffer[currentPacket.position++] = val;
                if (currentPacket.position < currentPacket.size - 2)
                    currentPacket.checksum += val;
            }
            if (currentPacket.position >= currentPacket.size)
            {
                bool isValid = true;
                if (currentPacket.checksum != currentPacket.buffer[currentPacket.size - 3])
                {
                    ESP_LOGW(TAG, "Wrong packet checksum: 0x%02X (expected 0x%02X)", currentPacket.checksum, currentPacket.buffer[currentPacket.size - 3]);
                    isValid = false;
                }
                else
                {
                    uint16_t crc = crc16(currentPacket.buffer, currentPacket.size - 3);
                    if (((crc >> 8) != currentPacket.buffer[currentPacket.size - 2]) || ((crc & 0xFF) != currentPacket.buffer[currentPacket.size - 1]))
                    {
                        ESP_LOGW(TAG, "Wrong packet checksum: 0x%04X (expected 0x%02X%02X)", crc, currentPacket.buffer[currentPacket.size - 2], currentPacket.buffer[currentPacket.size - 1]);
                        isValid = false;
                    }
                }
                if (isValid)
                {
                    // We got valid packet here, need to handle it
                    handleIncomingPacket();
               }
                currentPacket.Reset();
            }
        }
        else // Haven't found beginning of packet yet
        {
            if (val == HEADER)
                currentPacket.preHeaderCharsCounter++;
            else
            {
                if (currentPacket.preHeaderCharsCounter >= 2)
                {
                    if ((val + 3 <= MAX_MESSAGE_SIZE) and (val >= 8)) // Packet size should be at least 8
                    {
                        // Valid packet size
                        currentPacket.size = val + 3;    // Space for checksum
                        currentPacket.buffer[0] = val;
                        currentPacket.checksum += val;  // will calculate checksum on the fly
                        currentPacket.position = 1;
                        mLastByteTimestamp = std::chrono::steady_clock::now();   // Using timeout to make sure we not stuck
                    }
                    else
                        ESP_LOGW(TAG, "Wrong packet size %d", val);
                }
                currentPacket.preHeaderCharsCounter = 0;
            }
        }
    }
}

void HaierClimate::handleIncomingPacket()
{
    HaierPacketHeader& header = (HaierPacketHeader&)currentPacket.buffer;
    std::string packet_type;
    ProtocolPhases oldPhase = mPhase;
    int level = ESPHOME_LOG_LEVEL_DEBUG;
    bool wrongPhase = false;
    switch (header.msg_type)
    {
        case hpAnswerInit1:
            packet_type = "Init1 command answer";
            if (mPhase == psWaitingAnswerInit1)
                mPhase = psSendingInit2;
            else
                level = ESPHOME_LOG_LEVEL_WARN;
            break;
        case hpAnswerInit2:
            packet_type = "Init2 command answer";
            if (mPhase == psWaitingAnswerInit2)
                mPhase = psSendingFirstStatusRequest;
            else
                level = ESPHOME_LOG_LEVEL_WARN;
            break;
        case hpAnswerRequestStatus:
            packet_type = "Poll command answer";
            if (mPhase >= psWaitingFirstStatusAnswer) // Accept status on any stage after initialization
            {
                if (mPhase == psWaitingFirstStatusAnswer)
                    ESP_LOGI(TAG, "First status received");
                xSemaphoreTake(mReadMutex, portMAX_DELAY);
                memcpy(mLastPacket, currentPacket.buffer, currentPacket.size);
                xSemaphoreGive(mReadMutex);
                processStatus(currentPacket.buffer, currentPacket.size);
                if ((mPhase == psWaitingStatusAnswer) || (mPhase == psWaitingFirstStatusAnswer))
                    // Change phase only if we were waiting for status
                    mPhase = psIdle;
            }
            else
                level = ESPHOME_LOG_LEVEL_WARN;
            break;
        case hpAnswerError:
            packet_type = "Command error";
            level = ESPHOME_LOG_LEVEL_WARN;
            if (mPhase == psWaitingStatusAnswer)
                mPhase = psIdle;
            // No else to avoid to many requests, we will retry on timeout
            break;
        case hpAnswerReadyToSendSignal:
            packet_type = "Signal request answer";
            if (mPhase == psWaitingUpateSignalAnswer)
                mPhase = psSendingSignalLevel;
            else
                level = ESPHOME_LOG_LEVEL_WARN;
            break;
        default:
            packet_type = "Unknown";
            break;
    }
    std::string raw = getHex(currentPacket.buffer, currentPacket.size);
    ESP_LOG_L(level, TAG, "Received %s message during phase %d, size: %d, content: %02X %02X%s", packet_type.c_str(), oldPhase, currentPacket.size, HEADER, HEADER, raw.c_str());
}

void HaierClimate::sendData(const uint8_t * message, size_t size, bool withCrc)
{
    uint8_t packetSize = size + (withCrc ? 5 : 3);
    if (packetSize > MAX_MESSAGE_SIZE)
    {
        ESP_LOGE(TAG, "Message is to big: %d", size);
        return;
    }
    uint8_t buffer[MAX_MESSAGE_SIZE] = {0xFF, 0xFF};
    memcpy(buffer + 2, message, size);
    buffer[size + 2] = getChecksum(buffer + 2, size);
    if (withCrc)
    {
        uint16_t crc_16 = crc16(buffer + 2, size);
        buffer[size + 3] = crc_16 >> 8;
        buffer[size + 4] = crc_16 & 0xFF;
    }
    write_array(buffer, packetSize);
    ESP_LOGD(TAG, "Message sent:%s", getHex(buffer, packetSize).c_str());
}

ClimateTraits HaierClimate::traits()
{
    return mTraits;
}

void HaierClimate::control(const ClimateCall &call)
{
    static uint8_t controlOutBuffer[CONTROL_PACKET_SIZE];
    ESP_LOGD("Control", "Control call");
    if(mPhase <= psWaitingFirstStatusAnswer){
        ESP_LOGE("Control", "No action, first poll answer not received");
        return; //cancel the control, we cant do it without a poll answer.
    }
    memcpy(controlOutBuffer, &control_command, HEADER_SIZE);
    xSemaphoreTake(mReadMutex, portMAX_DELAY);
    memcpy(&controlOutBuffer[HEADER_SIZE], &mLastPacket[HEADER_SIZE], CONTROL_PACKET_SIZE - HEADER_SIZE);
    xSemaphoreGive(mReadMutex);
    HaierControl& outData = (HaierControl&) controlOutBuffer;
    outData.header.msg_type = 1;
    outData.header.arguments[0] = (uint8_t)hpCommandControl;
    outData.header.msg_length = CONTROL_PACKET_SIZE;
    if (call.get_mode().has_value())
    {
        switch (*call.get_mode())
        {
            case CLIMATE_MODE_OFF:
                outData.control.ac_power = 0;
                break;

            case CLIMATE_MODE_AUTO:
                outData.control.ac_power = 1;
                outData.control.ac_mode = ConditioningAuto;
                outData.control.fan_mode = mOtherModesFanSpeed;
                break;

            case CLIMATE_MODE_HEAT:
                outData.control.ac_power = 1;
                outData.control.ac_mode = ConditioningHeat;
                outData.control.fan_mode = mOtherModesFanSpeed;
                break;

            case CLIMATE_MODE_DRY:
                outData.control.ac_power = 1;
                outData.control.ac_mode = ConditioningDry;
                outData.control.fan_mode = mOtherModesFanSpeed;
                break;

            case CLIMATE_MODE_FAN_ONLY:
                outData.control.ac_power = 1;
                outData.control.ac_mode = ConditioningFan;
                outData.control.fan_mode = mFanModeFanSpeed;    // Auto doesn't work in fan only mode
                break;

            case CLIMATE_MODE_COOL:
                outData.control.ac_power = 1;
                outData.control.ac_mode = ConditioningCool;
                outData.control.fan_mode = mOtherModesFanSpeed;
                break;
            default:
                ESP_LOGE("Control", "Unsupported climate mode");
                return;
        }
    }
    //Set fan speed, if we are in fan mode, reject auto in fan mode
    if (call.get_fan_mode().has_value())
    {
        switch(call.get_fan_mode().value())
        {
            case CLIMATE_FAN_LOW:
                outData.control.fan_mode = FanLow;
                break;
            case CLIMATE_FAN_MEDIUM:
                outData.control.fan_mode = FanMid;
                break;
            case CLIMATE_FAN_HIGH:
                outData.control.fan_mode = FanHigh;
                break;
            case CLIMATE_FAN_AUTO:
                if (mode != CLIMATE_MODE_FAN_ONLY) //if we are not in fan only mode
                    outData.control.fan_mode = FanAuto;
                break;
            default:
                ESP_LOGE("Control", "Unsupported fan mode");
                return;
        }
    }
    //Set swing mode
    if (call.get_swing_mode().has_value())
    {
        switch(call.get_swing_mode().value())
        {
            case CLIMATE_SWING_OFF:
                outData.control.horizontal_swing_mode = HorizontalSwingCenter;
                outData.control.vertical_swing_mode = VerticalSwingCenter;
                break;
            case CLIMATE_SWING_VERTICAL:
                outData.control.horizontal_swing_mode = HorizontalSwingCenter;
                outData.control.vertical_swing_mode = VerticalSwingAuto;
                break;
            case CLIMATE_SWING_HORIZONTAL:
                outData.control.horizontal_swing_mode = HorizontalSwingAuto;
                outData.control.vertical_swing_mode = VerticalSwingCenter;
                break;
            case CLIMATE_SWING_BOTH:
                outData.control.horizontal_swing_mode = HorizontalSwingAuto;
                outData.control.vertical_swing_mode = VerticalSwingAuto;
                break;
        }
    }
    if (call.get_target_temperature().has_value())
        outData.control.set_point = *call.get_target_temperature() - 16; //set the temperature at our offset, subtract 16.
    if (call.get_preset().has_value())
    {
        switch(call.get_preset().value())
        {
            case CLIMATE_PRESET_NONE:
                outData.control.away_mode   = 0;
                outData.control.quiet_mode  = 0;
                outData.control.fast_mode   = 0;
                outData.control.sleep_mode  = 0;
                break;
            case CLIMATE_PRESET_ECO:
                outData.control.away_mode   = 0;
                outData.control.quiet_mode  = 1;
                outData.control.fast_mode   = 0;
                outData.control.sleep_mode  = 0;
                break;
            case CLIMATE_PRESET_BOOST:
                outData.control.away_mode   = 0;
                outData.control.quiet_mode  = 0;
                outData.control.fast_mode   = 1;
                outData.control.sleep_mode  = 0;
                break;
            case CLIMATE_PRESET_AWAY:
                outData.control.away_mode   = 1;
                outData.control.quiet_mode  = 0;
                outData.control.fast_mode   = 0;
                outData.control.sleep_mode  = 0;
                break;
            case CLIMATE_PRESET_SLEEP:
                outData.control.away_mode   = 0;
                outData.control.quiet_mode  = 0;
                outData.control.fast_mode   = 0;
                outData.control.sleep_mode  = 1;
                break;
            default:
                ESP_LOGE("Control", "Unsupported preset");
                return;
        }
    }
    sendData(controlOutBuffer, controlOutBuffer[0]);
}

void HaierClimate::processStatus(const uint8_t* packetBuffer, uint8_t size)
{
    HaierStatus* packet = (HaierStatus*) packetBuffer;
    ESP_LOGD(TAG, "HVAC Mode = 0x%X", packet->control.ac_mode);
    ESP_LOGD(TAG, "Fan speed Status = 0x%X", packet->control.fan_mode);
    ESP_LOGD(TAG, "Horizontal Swing Status = 0x%X", packet->control.horizontal_swing_mode);
    ESP_LOGD(TAG, "Vertical Swing Status = 0x%X", packet->control.vertical_swing_mode);
    ESP_LOGD(TAG, "Set Point Status = 0x%X", packet->control.set_point);
    //extra modes/presets
    if (packet->control.quiet_mode != 0)
           preset = CLIMATE_PRESET_ECO;
    else if (packet->control.fast_mode != 0)
        preset = CLIMATE_PRESET_BOOST;
    else if (packet->control.sleep_mode != 0)
        preset = CLIMATE_PRESET_SLEEP;
    else if(packet->control.away_mode != 0)
        preset = CLIMATE_PRESET_AWAY;
    else
        preset = CLIMATE_PRESET_NONE;

    if (preset == CLIMATE_PRESET_AWAY)
        target_temperature = 10;    //away mode sets to 10c
    else
        target_temperature = packet->control.set_point + 16;
    current_temperature = packet->sensors.room_temperature / 2;
    //remember the fan speed we last had for climate vs fan
    if (packet->control.ac_mode ==  ConditioningFan)
        mFanModeFanSpeed = packet->control.fan_mode;
    else
        mOtherModesFanSpeed = packet->control.fan_mode;
    switch (packet->control.fan_mode)
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
    //climate mode
    if (packet->control.ac_power == 0)
        mode = CLIMATE_MODE_OFF;
    else
    {
        // Check current hvac mode
        switch (packet->control.ac_mode)
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
    // Swing mode
    if (packet->control.horizontal_swing_mode == HorizontalSwingAuto)
    {
        if (packet->control.vertical_swing_mode == VerticalSwingAuto)
            swing_mode = CLIMATE_SWING_BOTH;
        else
            swing_mode = CLIMATE_SWING_HORIZONTAL;
    }
    else
    {
        if (packet->control.vertical_swing_mode == VerticalSwingAuto)
            swing_mode = CLIMATE_SWING_VERTICAL;
        else
            swing_mode = CLIMATE_SWING_OFF;
    }
    mLastValidStatusTimestamp = std::chrono::steady_clock::now();
    this->publish_state();
}

} // namespace haier
} // namespace esphome
