#include "esphome.h"
#include <chrono>
#include <sstream>
#include <mutex>
#include "HaierClimate.h"
#include "HaierPacket.h"
#include <esp_wifi.h>

using namespace esphome;
using namespace esphome::climate;
using namespace esphome::uart;

#include <string>

#define PACKET_TIMOUT_MS            500
#define ANSWER_TIMOUT_MS            1000
#define COMMUNICATION_TIMOUT_MS            60000


std::string HaierPacketToString(const HaierPacketStatus& packet)
{
    std::stringstream sstream;
    sstream << "Message type: " << (uint)packet.header.msg_type << "\n"
            << "\tSetpoint: " << (uint)packet.control.set_point + 16 << "\n"
            << "\tAC mode: " << (uint)packet.control.ac_mode << "\n"
            << "\tFan mode: " << (uint)packet.control.fan_mode << "\n"
            << "\tVertical swing mode: " << (uint)packet.control.vertical_swing_mode << "\n"
            << "\tHorizontal swing mode: " << (uint)packet.control.horizontal_swing_mode << "\n"
            << "\tRoom temperature: " << (uint)packet.sensors.room_temperature / 2 << "\n"
            << "\tOutdoor temperature: " << (uint)packet.sensors.outdoor_temperature / 2 << "\n"
            << "\tIs on?: " << ((packet.control.ac_power != 0) ? "Yes" : "No");
    return sstream.str();
}

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

uint8_t initialization_1[15]    = {0x0A,    0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   INIT_COMMAND,   0x00,   0x07};                  //enables coms? magic message, needs some timing pause after this?
uint8_t type_poll[12]           = {0x08,    0x40,   0x00,   0x00,   0x00,   0x00,   0x00,   SEND_TYPE_POLL};                                //sets message type, we only use the poll type for now
uint8_t type_wifi[12]           = {0x08,    0x40,   0x00,   0x00,   0x00,   0x00,   0x00,   SEND_TYPE_WIFI};
uint8_t wifi_status[17]         = {0x0C,    0x40,   0x00,   0x00,   0x00,   0x00,   0x00,   SEND_WIFI,      0x00,   0x00,   0x00,   0x32};  //last byte is signal strength, second to last goes to 1 when trying to connect, not used for now
uint8_t poll_command[15]        = {0x0A,    0x40,   0x00,   0x00,   0x00,   0x00,   0x00,   SEND_POLL,      0x4D,   0x01};                  //ask for the status


HaierClimate::HaierClimate(UARTComponent* parent) :
                                        PollingComponent(5 * 1000),
                                        UARTDevice(parent) ,
                                        mIsFirstStatusReceived(false),
                                        mFanModeFanSpeed(FanMid),
                                        mOtherModesFanSpeed(FanAuto)
{
    mLastPacket = new uint8_t[MAX_MESSAGE_SIZE];
    mReadMutex = xSemaphoreCreateMutex();
}

HaierClimate::~HaierClimate()
{
    vSemaphoreDelete(mReadMutex);
    delete[] mLastPacket;
}

void HaierClimate::setup()
{
    ESP_LOGI("Haier", "Haier initialization...");
    delay(2000);                                        //wait for the ac to boot, esp boots faster then the AC
    sendData(initialization_1, initialization_1[0]);    //start the comms?
    sendData(type_poll, type_poll[0]);                  //enable polling and commands?
    delay(2000);                                        //wait for the ac, if we don't wait it wont be ready, and it wont init.
}

struct CurrentPacketStatus
{
    uint8_t buffer[MAX_MESSAGE_SIZE];
    uint8_t preHeaderCharsCounter;
    uint8_t size;
    uint8_t position;
    std::chrono::steady_clock::time_point readTimeout;
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

void HaierClimate::loop()
{
    if ((currentPacket.size > 0) && (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - currentPacket.readTimeout).count() > PACKET_TIMOUT_MS))
    {
        ESP_LOGW("Haier", "Incoming packet timeout, packet size %d, expected size %d", currentPacket.position, currentPacket.size);
        currentPacket.Reset();
    }
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
                    ESP_LOGW("Haier", "Wrong packet checksum: 0x%02X (expected 0x%02X)", currentPacket.checksum, currentPacket.buffer[currentPacket.size - 3]);
                    isValid = false;
                }
                else
                {
                    uint16_t crc = crc16(currentPacket.buffer, currentPacket.size - 3);
                    if (((crc >> 8) != currentPacket.buffer[currentPacket.size - 2]) || ((crc & 0xFF) != currentPacket.buffer[currentPacket.size - 1]))
                    {
                        ESP_LOGW("Haier", "Wrong packet checksum: 0x%04X (expected 0x%02X%02X)", crc, currentPacket.buffer[currentPacket.size - 2], currentPacket.buffer[currentPacket.size - 1]);
                        isValid = false;
                    }
                }
                if (isValid)
                {
                    HaierPacketHeader& header = reinterpret_cast<HaierPacketHeader&>(currentPacket.buffer);
                    std::string packet_type;
                    switch (header.msg_type)
                    {
                        case RESPONSE_POLL:
                            packet_type = "Status";
                            xSemaphoreTake(mReadMutex, portMAX_DELAY);
                            memcpy(mLastPacket, currentPacket.buffer, currentPacket.size);
                            xSemaphoreGive(mReadMutex);
//                          {
//                              HaierPacketStatus& _packet = reinterpret_cast<HaierPacketStatus&>(currentPacket.buffer);
//                              ESP_LOGD("Control", HaierPacketToString(_packet).c_str());
//                          }
                            processStatus(currentPacket.buffer, currentPacket.size);
                            break;
                        case RESPONSE_TYPE_WIFI:
                            // Not supported yet
                            packet_type = "Response to WiFi Type";
                            break;
                        case RESPONSE_TYPE_POLL:
                            // Not supported yet
                            packet_type = "Response to Poll Type";
                            break;
                        case ERROR_POLL:
                            // Not supported yet
                            packet_type = "Error";
                            break;
                        default:
                            // Unknown message
                            packet_type = "Unknown";
                            break;
                    }
                    std::string raw = getHex(currentPacket.buffer, currentPacket.size);
                    ESP_LOGD("Haier", "Received %s message, size: %d, content: %02X %02X%s", packet_type.c_str(), currentPacket.size, HEADER, HEADER, raw.c_str());
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
                    if ((val + 3 <= MAX_MESSAGE_SIZE) and (val >= 10)) // Packet size should be at least 10
                    {
                        // Valid packet size
                        currentPacket.size = val + 3;    // Space for checksum
                        currentPacket.buffer[0] = val;
                        currentPacket.checksum += val;  // will calculate checksum on the fly
                        currentPacket.position = 1;
                        currentPacket.readTimeout = std::chrono::steady_clock::now();   // Using timeout to make sure we not stuck
                    }
                    else
                        ESP_LOGW("Haier", "Wrong packet size %d", val);
                }
                currentPacket.preHeaderCharsCounter = 0;
            }
        }
    }
}

void HaierClimate::update()
{
    ESP_LOGD("Haier", "POLLING");
    if (!mIsFirstStatusReceived) {
        //if we haven't gotten our first status
        ESP_LOGD("Haier", "First status not received");
    }
    sendData(poll_command, poll_command[0]);
	if (mIsFirstStatusReceived)
	{
		wifi_ap_record_t ap_info;
		if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
		{
			ESP_LOGD("Haier", "WiFi signal is: %d", ap_info.rssi);
		}
	}
}

void HaierClimate::sendData(const uint8_t * message, size_t size)
{
    uint8_t crc = getChecksum(message, size);
    uint16_t crc_16 = crc16(message, size);
    write_byte(HEADER);
    write_byte(HEADER);
    write_array(message, size);
    write_byte(crc);
    write_byte(crc_16 >> 8);
    write_byte(crc_16 & 0xFF);
    auto raw = getHex(message, size);
    ESP_LOGD("Haier", "Message sent: %02X %02X%s %02X %02X %02X", HEADER, HEADER, raw.c_str(), crc, (crc_16 >> 8), crc_16 & 0xFF);
}

ClimateTraits HaierClimate::traits()
{
    ESP_LOGD("Haier", "traits call");
    auto traits = climate::ClimateTraits();
    traits.set_supported_modes(
    {
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_HEAT,
        climate::CLIMATE_MODE_FAN_ONLY,
        climate::CLIMATE_MODE_DRY,
        climate::CLIMATE_MODE_AUTO
    });
    traits.set_supported_fan_modes(
    {
        climate::CLIMATE_FAN_AUTO,
        climate::CLIMATE_FAN_LOW,
        climate::CLIMATE_FAN_MEDIUM,
        climate::CLIMATE_FAN_HIGH,
    });
    traits.set_supported_swing_modes(
    {
        climate::CLIMATE_SWING_OFF,
        climate::CLIMATE_SWING_BOTH,
        climate::CLIMATE_SWING_VERTICAL,
        climate::CLIMATE_SWING_HORIZONTAL
    });
    traits.set_supported_presets(
    {
        climate::CLIMATE_PRESET_NONE,
        climate::CLIMATE_PRESET_ECO,
        climate::CLIMATE_PRESET_BOOST,
        climate::CLIMATE_PRESET_SLEEP,
        climate::CLIMATE_PRESET_AWAY

    });
    traits.set_visual_min_temperature(MIN_SET_TEMPERATURE);
    traits.set_visual_max_temperature(MAX_SET_TEMPERATURE);
    traits.set_visual_temperature_step(0.5f); //displays current temp in 0.5 degree steps, we cannot change it in 0.5 degree steps however
    traits.set_supports_current_temperature(true);
    return traits;
}

void HaierClimate::control(const ClimateCall &call)
{
    static uint8_t controlOutBuffer[CONTROL_PACKET_SIZE];
    ESP_LOGD("Control", "Control call");
    if(mIsFirstStatusReceived == false){
        ESP_LOGE("Control", "No action, first poll answer not received");
        return; //cancel the control, we cant do it without a poll answer.
    }
    xSemaphoreTake(mReadMutex, portMAX_DELAY);
    memcpy(controlOutBuffer, mLastPacket, CONTROL_PACKET_SIZE);
    xSemaphoreGive(mReadMutex);
    HaierPacketStatus& outData = (HaierPacketStatus&) controlOutBuffer;
    outData.header.msg_type = 1;
    outData.header.msg_command = SEND_COMMAND;
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
    ESP_LOGD("Control", HaierPacketToString(outData).c_str());
    //sendData(controlOutBuffer, controlOutBuffer[0]);
}

void HaierClimate::processStatus(const uint8_t* packetBuffer, uint8_t size)
{
    HaierPacketStatus* packet = (HaierPacketStatus*) packetBuffer;
    ESP_LOGD("Debug", "HVAC Mode = 0x%X", packet->control.ac_mode);
    ESP_LOGD("Debug", "Fan speed Status = 0x%X", packet->control.fan_mode);
    ESP_LOGD("Debug", "Horizontal Swing Status = 0x%X", packet->control.horizontal_swing_mode);
    ESP_LOGD("Debug", "Vertical Swing Status = 0x%X", packet->control.vertical_swing_mode);
    ESP_LOGD("Debug", "Set Point Status = 0x%X", packet->control.set_point);
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
    this->publish_state();
	if (!mIsFirstStatusReceived)
		ESP_LOGI("Haier", "First status received");
    mIsFirstStatusReceived = true;
}
