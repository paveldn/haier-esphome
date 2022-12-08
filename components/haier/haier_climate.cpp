#include "esphome.h"
#include <chrono>
#include <string>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "haier_climate.h"
#include "haier_packet.h"
#include "esphome/components/wifi/wifi_component.h"

using namespace esphome::climate;
using namespace esphome::uart;

namespace esphome {
namespace haier {

#define TAG "haier.climate"

#define PACKET_TIMEOUT_MS                300
#define ANSWER_TIMEOUT_MS                1000
#define COMMUNICATION_TIMEOUT_MS         60000
#define STATUS_REQUEST_INTERVAL_MS       5000
#define SIGNAL_LEVEL_UPDATE_INTERVAL_MS  10000

#define MAX_MESSAGE_SIZE                254
#define MAX_FRAME_SIZE                  (MAX_MESSAGE_SIZE + 4)
#define SEPARATOR_BYTE					0xFF
#define SEPARATOR_POST_BYTE				0x55
#define CRC_INIT_VAL					0x00

// ESP_LOG_LEVEL don't work as I want it so I implemented this macro
#define ESP_LOG_L(level, tag, format, ...) do {                     \
        if (level==ESPHOME_LOG_LEVEL_ERROR )        { ESP_LOGE(tag, format, __VA_ARGS__); } \
        else if (level==ESPHOME_LOG_LEVEL_WARN )    { ESP_LOGW(tag, format, __VA_ARGS__); } \
        else if (level==ESPHOME_LOG_LEVEL_INFO)     { ESP_LOGI(tag, format, __VA_ARGS__); } \
        else if (level==ESPHOME_LOG_LEVEL_DEBUG )   { ESP_LOGD(tag, format, __VA_ARGS__); } \
        else if (level==ESPHOME_LOG_LEVEL_VERBOSE ) { ESP_LOGV(tag, format, __VA_ARGS__); } \
        else                                        { ESP_LOGVV(tag, format, __VA_ARGS__); } \
    } while(0)

#define min(a,b) ((a)<(b)?(a):(b))

uint8_t getChecksum(const uint8_t * message, size_t size) 
{
	uint8_t result = 0;
	for (int i = 0; i < size; i++){
		result += message[i];
	}
	return result;
}

// CRC-16/ARC lookup table
static const uint16_t crc_table[] = {
		0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
		0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
		0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
		0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
		0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
		0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
		0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
		0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
		0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
		0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
		0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
		0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
		0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
		0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
		0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
		0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
		0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
		0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
		0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
		0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
		0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
		0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
		0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
		0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
		0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
		0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
		0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
		0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
		0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
		0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
		0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
		0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040,
	};


inline uint16_t crc16(const uint8_t data, uint16_t crc = 0)
{
	return (crc >> 8) ^ crc_table[(crc ^ data) & 0xFF];
}

uint16_t crc16(const uint8_t* const data, size_t size, uint16_t initial_val = 0)
{
	const uint8_t* val = data;
	uint16_t crc = initial_val;
	while (size--)
	{
		crc = crc16(*val, crc);
		++val;
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

VerticalSwingMode getVerticalSwingMode(AirflowVerticalDirection direction)
{
    switch (direction)
    {
        case vdUp:
            return VerticalSwingUp;
        case vdDown:
            return VerticalSwingDown;
        default:
            return VerticalSwingCenter;
    }
}

HorizontalSwingMode getHorizontalSwingMode(AirflowHorizontalDirection direction)
{
    switch (direction)
    {
        case hdLeft:
            return HorizontalSwingLeft;
        case hdRight:
            return HorizontalSwingRight;
        default:
            return HorizontalSwingCenter;
    }
}

HaierClimate::HaierClimate(UARTComponent* parent) :
                                        Component(),
                                        UARTDevice(parent),
                                        mPhase(psSendingInit1),
                                        mFanModeFanSpeed(FanMid),
                                        mOtherModesFanSpeed(FanAuto),
                                        mSendWifiSignal(false),
                                        mBeeperStatus(true),
                                        mDisplayStatus(true),
                                        mForceSendControl(false),
                                        mHvacHardwareInfoAvailable(false),
                                        mHvacFunctions{false, false, false, false, false},
                                        mUseCrc(mHvacFunctions[2]),
                                        mActiveAlarms{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                                        mOutdoorSensor(nullptr)
{
    mLastPacket = new uint8_t[MAX_FRAME_SIZE];
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
    mVerticalDirection = vdCenter;
    mHorizontalDirection = hdCenter;
}

HaierClimate::~HaierClimate()
{
    delete[] mLastPacket;
}

void HaierClimate::setPhase(ProtocolPhases phase)
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
    if ((direction < 0) || (direction > vdDown))
        mVerticalDirection = vdCenter;
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
    if ((direction < 0) || (direction > hdRight))
        mHorizontalDirection = hdCenter;
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

void HaierClimate::setup()
{
    ESP_LOGI(TAG, "Haier initialization...");
    delay(2000); // Give time for AC to boot
    setPhase(psSendingInit1);
}

void HaierClimate::dump_config()
{
    LOG_CLIMATE("", "Haier hOn Climate", this);
    if (mHvacHardwareInfoAvailable)
    {
        Lock _lock(mReadMutex);
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
            getHex(mActiveAlarms, sizeof(mActiveAlarms)).c_str());

    }
}


namespace   // anonymous namespace for local things
{
    struct CurrentPacketStatus
    {
        uint8_t buffer[MAX_FRAME_SIZE];
        uint8_t preHeaderCharsCounter;
        uint8_t size;
        uint8_t position;
        uint8_t checksum;
        bool    has_crc;
        CurrentPacketStatus() : preHeaderCharsCounter(0),
                                size(0),
                                position(0),
                                checksum(0),
                                has_crc(false)
        {};
        void Reset()
        {
            size = 0;
            position = 0;
            checksum = 0;
            preHeaderCharsCounter = 0;
            has_crc = false;
        };
    };

    CurrentPacketStatus currentPacket;

}

void HaierClimate::loop()
{
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if ((mPhase >= psIdle) && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastValidStatusTimestamp).count() > COMMUNICATION_TIMEOUT_MS))
    {
        ESP_LOGE(TAG, "No valid status answer for to long. Resetting protocol");
        currentPacket.Reset();
        setPhase(psSendingInit1);
        return;
    }
    switch (mPhase)
    {
        case psSendingInit1:
            {
                mHvacHardwareInfoAvailable = false;
                // Indicate device capabilities:
                // bit 0 - if 1 module support interactive mode
                // bit 1 - if 1 module support master-slave mode
                // bit 2 - if 1 module support crc
                // bit 3 - if 1 module support multiple devices
                // bit 4..bit 15 - not used
                uint8_t module_capabilities[2] = { 0b00000000, 0b00000111 };     
                sendFrame(HaierProtocol::ftGetDeviceVersion, module_capabilities, sizeof(module_capabilities));
                setPhase(psWaitingAnswerInit1);
                mLastRequestTimestamp = now;
            }
            return;
        case psSendingInit2:
            sendFrame(HaierProtocol::ftGetDeviceId);
            setPhase(psWaitingAnswerInit2);
            mLastRequestTimestamp = now;
            return;
        case psSendingFirstStatusRequest:
        case psSendingStatusRequest:
            sendFrameWithSubcommand(HaierProtocol::ftControl, HaierProtocol::stControlGetUserData);
            mLastStatusRequest = now;
            setPhase((ProtocolPhases)((uint8_t)mPhase + 1));
            mLastRequestTimestamp = now;
            return;
        case psSendingAlarmStatusReauest:
            sendFrame(HaierProtocol::ftGetAlarmStatus);
            setPhase(psWaitingAlarmStatusAnswer); 
            mLastRequestTimestamp = now;
            return;
        case psSendingUpdateSignalRequest:
            sendFrame(HaierProtocol::ftGetManagementInformation);
            mLastSignalRequest = now;
            setPhase(psWaitingUpateSignalAnswer);
            mLastRequestTimestamp = now;
            return;
        case psSendingSignalLevel:
            if (mSendWifiSignal)
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
                sendFrame(HaierProtocol::ftReportNetworkStatus, wifi_status_data, sizeof(wifi_status_data));
                setPhase(psWaitingSignalLevelAnswer);
            }
            else
                setPhase(psIdle);    // We don't expect answer here
            return;
        case psWaitingAnswerInit1:
        case psWaitingAnswerInit2:
        case psWaitingFirstStatusAnswer:
        case psWaitingAlarmStatusAnswer:
            // Using status request interval here to avoid pushing to many messages if AC is not ready
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastRequestTimestamp).count() > STATUS_REQUEST_INTERVAL_MS)
            {
                // No valid communication yet, resetting protocol,
                // No logs to avoid to many messages
                currentPacket.Reset();
                setPhase(psSendingInit1);
                return;
            }
            break;
        case psWaitingStatusAnswer:
        case psWaitingUpateSignalAnswer:
        case psWaitingSignalLevelAnswer:
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastRequestTimestamp).count() > ANSWER_TIMEOUT_MS)
            {
                // We have valid communication here, no problem if we missed packet or two
                // Just request packet again in next loop call. We also protected by protocol timeout
                ESP_LOGW(TAG, "Request answer timeout, phase %d", mPhase);
                setPhase(psIdle);
                return;
            }
            break;
        case psIdle:
            if (mForceSendControl)
            {
                sendControlPacket();
                mForceSendControl = false;
            }
            break;
        default:
            // Shouldn't get here
            ESP_LOGE(TAG, "Wrong protocol handler state: %d, resetting communication", mPhase);
            currentPacket.Reset();
            setPhase(psSendingInit1);
            return;
    }
    // Here we expect some input from AC or just waiting for the proper time to send the request
    // Anyway we read the port to make sure that the buffer does not overflow
    if ((currentPacket.size > 0) && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastByteTimestamp).count() > PACKET_TIMEOUT_MS))
    {
        ESP_LOGW(TAG, "Incoming packet timeout, packet size %d, expected size %d", currentPacket.position, currentPacket.size);
        currentPacket.Reset();
    }
    getSerialData();
    if (mPhase == psIdle)
    {
        // If we not waiting for answers check if there is a proper time to request data
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastStatusRequest).count() > STATUS_REQUEST_INTERVAL_MS)
            setPhase(psSendingStatusRequest);
        else if (mSendWifiSignal && (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastSignalRequest).count() > SIGNAL_LEVEL_UPDATE_INTERVAL_MS))
            setPhase(psSendingUpdateSignalRequest);
    }
}

void HaierClimate::getSerialData()
{
    while (available() > 0)
    {
        uint8_t val;
		bool prev_separator = false;
		bool ignore_byte = false;
        if (!read_byte(&val))
            break;
        if (currentPacket.size > 0) // Already found packet start
        {
			// Skip 0x55 after 0xFF if it is not packets separator
			ignore_byte = prev_separator && (val == SEPARATOR_POST_BYTE);
			prev_separator = val == SEPARATOR_BYTE;
            if (currentPacket.position < currentPacket.size)
            {
				if (!ignore_byte)
					currentPacket.buffer[currentPacket.position++] = val;
                if (currentPacket.position == 2) // Found crc indication flags
                {
                    currentPacket.has_crc = (val & 0x40) != 0;
                    if (currentPacket.has_crc)
                        currentPacket.size += 2;    // Make space for CRC
                }
                if (currentPacket.position < currentPacket.size - (currentPacket.has_crc ? 2 : 0))
					// Not checking ignore_byte here, extra 0x55 should be included in checksum but not CRC
                    currentPacket.checksum += val;
            }
            if (currentPacket.position >= currentPacket.size)
            {
                ESP_LOGV(TAG, "Packet received: FF FF%s", getHex(currentPacket.buffer, currentPacket.size).c_str());
                bool isValid = true;
                if (currentPacket.checksum != currentPacket.buffer[currentPacket.size - (currentPacket.has_crc ? 3 : 1)])
                {
                    ESP_LOGW(TAG, "Wrong packet checksum: 0x%02X (expected 0x%02X)", currentPacket.checksum, currentPacket.buffer[currentPacket.size - (currentPacket.has_crc ? 3 : 1)]);
                    isValid = false;
                }
                else if (currentPacket.has_crc)
                {
                    uint16_t crc = crc16(currentPacket.buffer, currentPacket.size - 3);
                    if (((crc >> 8) != currentPacket.buffer[currentPacket.size - 2]) || ((crc & 0xFF) != currentPacket.buffer[currentPacket.size - 1]))
                    {
                        ESP_LOGW(TAG, "Wrong packet CRC: 0x%04X (expected 0x%02X%02X)", crc, currentPacket.buffer[currentPacket.size - 2], currentPacket.buffer[currentPacket.size - 1]);
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
            if (val == SEPARATOR_BYTE)
                currentPacket.preHeaderCharsCounter++;
            else
            {
                if (currentPacket.preHeaderCharsCounter >= 2)
                {
                    if ((val + 1 <= MAX_MESSAGE_SIZE) and (val >= 8)) // Packet size should be at least 8
                    {
                        // Valid packet size
                        currentPacket.size = val + 1;
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
    HaierPacketHeader* header = (HaierPacketHeader*)currentPacket.buffer;
    std::string packet_type;
    ProtocolPhases oldPhase = mPhase;
    int level = ESPHOME_LOG_LEVEL_DEBUG;
    bool wrongPhase = false;
    switch (header->frame_type)
    {
        case HaierProtocol::ftGetDeviceVersionResponse:
            packet_type = "Init1 command answer";
            if (mPhase == psWaitingAnswerInit1)
            {
                DeviceVersionAnswer* answr = (DeviceVersionAnswer*)&currentPacket.buffer[HEADER_SIZE];
                char tmp[9];
                tmp[8] = 0;
                {
                    Lock _lock(mReadMutex);
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
                }
                mHvacHardwareInfoAvailable = true;
                setPhase(psSendingInit2);
            }
            else
                level = ESPHOME_LOG_LEVEL_WARN;
            break;
        case HaierProtocol::ftGetAlarmStatusResponse:
            packet_type = "Alarm status answer";
            if (mPhase == psWaitingAlarmStatusAnswer)
            {
                {
                    Lock _lock(mReadMutex);
                    memcpy(mActiveAlarms, &currentPacket.buffer[HEADER_SIZE + 2], 8);
                }
                setPhase(psIdle);
            }
            else
                level = ESPHOME_LOG_LEVEL_WARN;
            break;
        case HaierProtocol::ftGetDeviceIdResponse:
            packet_type = "Init2 command answer";
            if (mPhase == psWaitingAnswerInit2)
                setPhase(psSendingFirstStatusRequest);
            else
                level = ESPHOME_LOG_LEVEL_WARN;
            break;
        case HaierProtocol::ftStatus:
            packet_type = "Poll command answer";
            if ((mPhase == psWaitingFirstStatusAnswer) || (mPhase >= psIdle)) // Accept status on any stage after initialization
            {
                if (mPhase == psWaitingFirstStatusAnswer)
                    ESP_LOGI(TAG, "First status received");
                {
                    Lock _lock(mReadMutex);
                    memcpy(mLastPacket, currentPacket.buffer, currentPacket.size);
                }
                processStatus(currentPacket.buffer, currentPacket.size);
                // Change phase only if we were waiting for status
                if (mPhase == psWaitingFirstStatusAnswer)
                    setPhase(psSendingAlarmStatusReauest);
                else if (mPhase == psWaitingStatusAnswer)
                    setPhase(psIdle);
            }
            else
                level = ESPHOME_LOG_LEVEL_WARN;
            break;
        case HaierProtocol::ftConfirm:
            packet_type = "Confirmation";
            if (mPhase == psWaitingSignalLevelAnswer)
                setPhase(psIdle);
            else
                level = ESPHOME_LOG_LEVEL_WARN;
            break;
        case HaierProtocol::ftInvalid:
            packet_type = "Command error";
            level = ESPHOME_LOG_LEVEL_WARN;
            if (mPhase == psWaitingStatusAnswer)
                setPhase(psIdle);
            // No else to avoid to many requests, we will retry on timeout
            break;
        case HaierProtocol::ftGetManagementInformationResponse:
            packet_type = "Signal request answer";
            if (mPhase == psWaitingUpateSignalAnswer)
                setPhase(psSendingSignalLevel);
            else
                level = ESPHOME_LOG_LEVEL_WARN;
            break;
        default:
            packet_type = "Unknown";
            break;
    }
    std::string raw = getHex(currentPacket.buffer, currentPacket.size);
    ESP_LOG_L(level, TAG, "Received %s message during phase %d, size: %d, content: %02X %02X%s", packet_type.c_str(), oldPhase, currentPacket.size, SEPARATOR_BYTE, SEPARATOR_BYTE, raw.c_str());
}

void HaierClimate::sendFrame(HaierProtocol::FrameType type, const uint8_t* data, size_t data_size)
{
    sendFrameWithSubcommand(type, NO_SUBCOMMAND, data, data_size);
}

void HaierClimate::sendFrameWithSubcommand(HaierProtocol::FrameType type, uint16_t subcommand, const uint8_t* data, size_t data_size)
{
    if ((data == NULL) && (data_size > 0))
    {
        ESP_LOGE(TAG, "Wrong data size: %d", data_size);
        return;
    }
    uint8_t msg_size = HEADER_SIZE + (subcommand != NO_SUBCOMMAND ? 2 : 0) + data_size;
    size_t frameSize = 2 + msg_size + 1 + (mUseCrc ? 2 : 0);
    if (frameSize > MAX_FRAME_SIZE)
    {
        ESP_LOGE(TAG, "Frame is to big: %d", frameSize);
        return;
    }
    uint8_t buffer[MAX_FRAME_SIZE * 2] = {SEPARATOR_BYTE, SEPARATOR_BYTE};
    size_t pos = 2;
	uint8_t checksum = 0;
	uint16_t crc = CRC_INIT_VAL;
	uint8_t header_buf[HEADER_SIZE] = {0};
    HaierPacketHeader* header = (HaierPacketHeader*)header_buf;
    header->msg_length = msg_size;
    header->crc_present = mUseCrc ? 0x40 : 0x00;
    memset(header->reserved, 0, sizeof(header->reserved));
    header->frame_type = type;
#define PROCESS_BYTE_FOR_PACKET(_buffer, _pos, _value) do { \
				_buffer[_pos++] = _value; \
				checksum += _value; \
				if (mUseCrc) \
					crc = crc16(_value, crc); \
				if (_value == SEPARATOR_BYTE) \
				{ \
					_buffer[_pos++] = SEPARATOR_POST_BYTE; \
					checksum += SEPARATOR_POST_BYTE; \
					++frameSize; \
				} \
			} while (0)
	for (int i = 0; i < sizeof(header_buf); i++)
		PROCESS_BYTE_FOR_PACKET(buffer, pos, header_buf[i]);
    if (subcommand != NO_SUBCOMMAND)
    {
		uint8_t val = subcommand >> 8;
		PROCESS_BYTE_FOR_PACKET(buffer, pos, val);
		val = subcommand & 0xFF;
		PROCESS_BYTE_FOR_PACKET(buffer, pos, val);
    }
    if (data_size > 0)
    {
		for (int i = 0; i < data_size; i++)
			PROCESS_BYTE_FOR_PACKET(buffer, pos, data[i]);
    }
#undef PROCESS_BYTE_FOR_PACKET
    buffer[pos++] = checksum;
	if (checksum == SEPARATOR_BYTE)
	{
		buffer[pos++] = SEPARATOR_POST_BYTE;
		++frameSize;
	}
    if (mUseCrc)
    {
		uint8_t val = crc >> 8;
		buffer[pos++] = val;
		if (val == SEPARATOR_BYTE)
		{
			buffer[pos++] = SEPARATOR_POST_BYTE;
			++frameSize;
		}
		val = crc & 0xFF;
		buffer[pos++] = val;
		if (val == SEPARATOR_BYTE)
		{
			buffer[pos++] = SEPARATOR_POST_BYTE;
			++frameSize;
		}
    }
    write_array(buffer, frameSize);
    ESP_LOGD(TAG, "Message sent:%s", getHex(buffer, frameSize).c_str());
}

ClimateTraits HaierClimate::traits()
{
    return mTraits;
}

void HaierClimate::sendControlPacket(const ClimateCall* climateControl)
{
    if(mPhase <= psWaitingFirstStatusAnswer)
    {
        ESP_LOGE(TAG, "sendControlPacket: Can't send control packet, first poll answer not received");
        return; //cancel the control, we cant do it without a poll answer.
    }
    uint8_t controlOutBuffer[sizeof(HaierPacketControl)];
    {
        Lock _lock(mReadMutex);
        memcpy(controlOutBuffer, &mLastPacket[HEADER_SIZE + 2], sizeof(HaierPacketControl));
    }
    HaierPacketControl* outData = (HaierPacketControl*) controlOutBuffer;
    if (climateControl != NULL)
    {
        if (climateControl->get_mode().has_value())
        {
            switch (*climateControl->get_mode())
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
                    return;
            }
        }
        //Set fan speed, if we are in fan mode, reject auto in fan mode
        if (climateControl->get_fan_mode().has_value())
        {
            switch(climateControl->get_fan_mode().value())
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
                    return;
            }
        }
        //Set swing mode
        if (climateControl->get_swing_mode().has_value())
        {
            switch(climateControl->get_swing_mode().value())
            {
                case CLIMATE_SWING_OFF:
                    outData->horizontal_swing_mode = getHorizontalSwingMode(mHorizontalDirection);
                    outData->vertical_swing_mode = getVerticalSwingMode(mVerticalDirection);
                    break;
                case CLIMATE_SWING_VERTICAL:
                    outData->horizontal_swing_mode = getHorizontalSwingMode(mHorizontalDirection);
                    outData->vertical_swing_mode = VerticalSwingAuto;
                    break;
                case CLIMATE_SWING_HORIZONTAL:
                    outData->horizontal_swing_mode = HorizontalSwingAuto;
                    outData->vertical_swing_mode = getVerticalSwingMode(mVerticalDirection);
                    break;
                case CLIMATE_SWING_BOTH:
                    outData->horizontal_swing_mode = HorizontalSwingAuto;
                    outData->vertical_swing_mode = VerticalSwingAuto;
                    break;
            }
        }
        if (climateControl->get_target_temperature().has_value())
            outData->set_point = *climateControl->get_target_temperature() - 16; //set the temperature at our offset, subtract 16.
        if (climateControl->get_preset().has_value())
        {
            switch(climateControl->get_preset().value())
            {
                case CLIMATE_PRESET_NONE:
                    outData->quiet_mode  = 0;
                    outData->fast_mode   = 0;
                    outData->sleep_mode  = 0;
                    break;
                case CLIMATE_PRESET_ECO:
                    outData->quiet_mode  = 1;
                    outData->fast_mode   = 0;
                    outData->sleep_mode  = 0;
                    break;
                case CLIMATE_PRESET_BOOST:
                    outData->quiet_mode  = 0;
                    outData->fast_mode   = 1;
                    outData->sleep_mode  = 0;
                    break;
                case CLIMATE_PRESET_AWAY:
                    outData->quiet_mode  = 0;
                    outData->fast_mode   = 0;
                    outData->sleep_mode  = 0;
                    break;
                case CLIMATE_PRESET_SLEEP:
                    outData->quiet_mode  = 0;
                    outData->fast_mode   = 0;
                    outData->sleep_mode  = 1;
                    break;
                default:
                    ESP_LOGE("Control", "Unsupported preset");
                    return;
            }
        }
    }
    else
    {
        if (outData->vertical_swing_mode != VerticalSwingAuto)
            outData->vertical_swing_mode = getVerticalSwingMode(mVerticalDirection);
        if (outData->horizontal_swing_mode != HorizontalSwingAuto)
            outData->horizontal_swing_mode = getHorizontalSwingMode(mHorizontalDirection);
    }
    outData->beeper_status = (!mBeeperStatus || (climateControl == NULL)) ? 1 : 0;
    controlOutBuffer[4] = 0;   // This byte should be cleared before setting values
    outData->display_status = mDisplayStatus ? 1 : 0;
    sendFrameWithSubcommand(HaierProtocol::ftControl, HaierProtocol::stControlSetGroupParameters, controlOutBuffer, sizeof(HaierPacketControl));
}

void HaierClimate::control(const ClimateCall &call)
{
    static uint8_t controlOutBuffer[CONTROL_PACKET_SIZE];
    ESP_LOGD("Control", "Control call");
    sendControlPacket(&call);
}

void HaierClimate::processStatus(const uint8_t* packetBuffer, uint8_t size)
{
    HaierStatus packet;
    memcpy(&packet, packetBuffer, min(size, sizeof(HaierStatus)));
    ESP_LOGD(TAG, "HVAC Mode = 0x%X", packet.control.ac_mode);
    ESP_LOGD(TAG, "Fan speed Status = 0x%X", packet.control.fan_mode);
    ESP_LOGD(TAG, "Horizontal Swing Status = 0x%X", packet.control.horizontal_swing_mode);
    ESP_LOGD(TAG, "Vertical Swing Status = 0x%X", packet.control.vertical_swing_mode);
    ESP_LOGD(TAG, "Set Point Status = 0x%X", packet.control.set_point);
    if (packet.sensors.error_status != 0)
    {
        ESP_LOGW(TAG, "HVAC error, code=0x%02X message: %s", 
            packet.sensors.error_status,
            (packet.sensors.error_status < sizeof(ErrorMessages)) ? ErrorMessages[packet.sensors.error_status].c_str() : "Unknown error");
    }
    if (mOutdoorSensor != nullptr)
    {
        float otemp = packet.sensors.outdoor_temperature + PROTOCOL_OUTDOOR_TEMPERATURE_OFFSET;
        if ((!mOutdoorSensor->has_state()) ||  (mOutdoorSensor->get_raw_state() != otemp))
          mOutdoorSensor->publish_state(otemp);
    }
    //extra modes/presets
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
    target_temperature = packet.control.set_point + 16;
    current_temperature = packet.sensors.room_temperature / 2.0f;
    //remember the fan speed we last had for climate vs fan
    if (packet.control.ac_mode ==  ConditioningFan)
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
    //climate mode
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
    // Swing mode
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
    mLastValidStatusTimestamp = std::chrono::steady_clock::now();
    this->publish_state();
}

} // namespace haier
} // namespace esphome
