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

#define TAG "Haier"

#define PACKET_TIMOUT_MS            	500
#define ANSWER_TIMOUT_MS            	1000
#define COMMUNICATION_TIMOUT_MS     	60000
#define STATUS_REQUEST_INTERVAL_MS		5000
#define SIGNAL_LEVEL_UPDATE_INTERVAL_MS	10000 

// temperatures supported by AC system
#define MIN_SET_TEMPERATURE         	16
#define MAX_SET_TEMPERATURE         	30

#define MAX_MESSAGE_SIZE            	64      //64 should be enough to cover largest messages we send and receive, for now.
#define HEADER                      	0xFF

std::string HaierPacketToString(const HaierControl& packet)
{
    std::stringstream sstream;
    sstream << "Message type: " << (uint)packet.header.msg_type << "\n"
            << "\tSetpoint: " << (uint)packet.control.set_point + 16 << "\n"
            << "\tAC mode: " << (uint)packet.control.ac_mode << "\n"
            << "\tFan mode: " << (uint)packet.control.fan_mode << "\n"
            << "\tVertical swing mode: " << (uint)packet.control.vertical_swing_mode << "\n"
            << "\tHorizontal swing mode: " << (uint)packet.control.horizontal_swing_mode << "\n"
            << "\tIs on?: " << ((packet.control.ac_power != 0) ? "Yes" : "No");
    return sstream.str();
}

std::string HaierPacketToString(const HaierStatus& packet)
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

namespace
{
	enum HaierProtocolCommands
	{
		hpCommandInit1				= 0x61,
		hpCommandInit2				= 0x70,
		hpCommandRequestStatus		= 0x01,
		hpCommandReadyToSendSignal	= 0xFC,
		hpCommandSignalLevel		= 0xF7,
		hpCommandControl			= 0x60,
	};

	enum HaierProtocolAnswers
	{
		hpAnswerInit1				= 0x62,
		hpAnswerInit2				= 0x71,
		hpAnswerRequestStatus		= 0x02,
		hpAnswerRequestStatusError	= 0x03,
		hpAnswerReadyToSendSignal	= 0xFD,
		hpCommandControlAnswer		= 0x61,
	};
}

uint8_t initialization_1[]  = {0x0A,    0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   (uint8_t)(hpCommandInit1),				0x00,   0x07};		//enables coms? magic message, needs some timing pause after this?
uint8_t initialization_2[]  = {0x08,	0x40,	0x00,	0x00,	0x00,	0x00,	0x00,	(uint8_t)(hpCommandInit2)};
uint8_t poll_command[]		= {0x0A,    0x40,   0x00,   0x00,   0x00,   0x00,   0x00,   (uint8_t)(hpCommandRequestStatus),		0x4D,   0x01};		//ask for the status
uint8_t wifi_request[]      = {0x08,    0x40,   0x00,   0x00,   0x00,   0x00,   0x00,   (uint8_t)(hpCommandReadyToSendSignal)};
uint8_t wifi_status[]      	= {0x0C,    0x40,   0x00,   0x00,   0x00,   0x00,   0x00,   (uint8_t)(hpCommandSignalLevel),		0x00,	0x00, 	0x00, 	0x00};

HaierClimate::HaierClimate(UARTComponent* parent) :
                                        Component(),
                                        UARTDevice(parent) ,
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
    ESP_LOGI(TAG, "Haier initialization...");
    delay(2000); // Give time for AC to boot
	mPhase = psSendingInit1;
}

namespace	// anonymous namespace for local things
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
			sendData(initialization_1, initialization_1[0]);
			mPhase = psWaitingAnswerInit1;
			mLastRequestTimestamp = now;
			return;
		case psSendingInit2:
			sendData(initialization_2, initialization_2[0]);
			mPhase = psWaitingAnswerInit2;
			mLastRequestTimestamp = now;
			return;
		case psSendingFirstStatusRequest:
		case psSendingStatusRequest:
			sendData(poll_command, poll_command[0]);
			mLastStatusRequest = now;
			mPhase = (ProtocolPhases)((uint8_t)mPhase + 1);
			mLastRequestTimestamp = now;
			return;
		case psSendingUpdateSignalRequest:
			sendData(wifi_request, wifi_request[0]);
			mLastSignalRequest = now;
			mPhase = psWaitingUpateSignalAnswer;
			mLastRequestTimestamp = now;
			return;
		case psSendingSignalLevel:
			{
				// TODO: Implement this
				wifi_ap_record_t ap_info;
				if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
					ESP_LOGD(TAG, "WiFi signal is: %ddBm => %d%%", ap_info.rssi, uint((128 + ap_info.rssi) / 1.28f));
				//sendData(wifi_status, wifi_status[0]);
			}
			mPhase = psIdle;	// We don't expect answer here
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
		else if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastSignalRequest).count() > SIGNAL_LEVEL_UPDATE_INTERVAL_MS)
			mPhase = psSendingUpdateSignalRequest;
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
                    if ((val + 3 <= MAX_MESSAGE_SIZE) and (val >= 10)) // Packet size should be at least 10
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
	HaierPacketHeader& header = reinterpret_cast<HaierPacketHeader&>(currentPacket.buffer);
	std::string packet_type;
	bool wrongPhase = false;
	switch (header.msg_type)
	{
		case hpAnswerInit1:
			packet_type = "Init1 command answer";
			if (mPhase == psWaitingAnswerInit1)
				mPhase = psSendingInit2;
			else
				wrongPhase = true;
			break;
		case hpAnswerInit2:
			packet_type = "Init2 command answer";
			if (mPhase == psWaitingAnswerInit2)
				mPhase = psSendingFirstStatusRequest;
			else
				wrongPhase = true;
			break;
		case hpAnswerRequestStatus:
			packet_type = "Poll command answer";
			if ((mPhase == psWaitingStatusAnswer) || (mPhase == psWaitingFirstStatusAnswer))
			{
				if (mPhase == psWaitingFirstStatusAnswer)
					ESP_LOGI(TAG, "First status received");
				xSemaphoreTake(mReadMutex, portMAX_DELAY);
				memcpy(mLastPacket, currentPacket.buffer, currentPacket.size);
				xSemaphoreGive(mReadMutex);
//                {
//                    HaierStatus& _packet = reinterpret_cast<HaierPacketStatus&>(currentPacket.buffer);
//                    ESP_LOGD("Control", HaierPacketToString(_packet).c_str());
//                }
				processStatus(currentPacket.buffer, currentPacket.size);
				mPhase = psIdle;
			}
			else
				wrongPhase = true;
			break;
		case hpAnswerRequestStatusError:
			packet_type = "Poll command error";
			if (mPhase == psWaitingAnswerInit2)
				// Reset poll timer here to avoid loop
				mPhase = psIdle;
			else
				wrongPhase = true;
			break;
		case hpAnswerReadyToSendSignal:
			packet_type = "Signal request answer";
			if (mPhase == psWaitingUpateSignalAnswer)
				mPhase = psSendingSignalLevel;
			else
				wrongPhase = true;
			break;
		default:
			packet_type = "Unknown";
			break;
	}
	std::string raw = getHex(currentPacket.buffer, currentPacket.size);
	if (wrongPhase)
	{
		// We shouldn't get here. If we are here it means that we received answer that we didn't expected
		// This can be caused by one of 2 reasons:
		// 		1. 	There is somebody else talking (for example we using ser2net connection with multiple clients)
		//			In this case we can just ignore those packets
		//		2.	AC became to slow and we receive packets that we already not waiting because of timeout
		//			Not sure what to do in this case
		ESP_LOGW(TAG, "Received %s message during wrong phase, size: %d, content: %02X %02X%s", packet_type.c_str(), currentPacket.size, HEADER, HEADER, raw.c_str());
	}
	else
		ESP_LOGD(TAG, "Received %s message, size: %d, content: %02X %02X%s", packet_type.c_str(), currentPacket.size, HEADER, HEADER, raw.c_str());

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
    ESP_LOGD(TAG, "Message sent: %02X %02X%s %02X %02X %02X", HEADER, HEADER, raw.c_str(), crc, (crc_16 >> 8), crc_16 & 0xFF);
}

ClimateTraits HaierClimate::traits()
{
    ESP_LOGD(TAG, "HaierClimate::traits called");
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
    if(mPhase <= psWaitingFirstStatusAnswer){
        ESP_LOGE("Control", "No action, first poll answer not received");
        return; //cancel the control, we cant do it without a poll answer.
    }
    xSemaphoreTake(mReadMutex, portMAX_DELAY);
    memcpy(controlOutBuffer, mLastPacket, CONTROL_PACKET_SIZE);
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
    ESP_LOGD("Control", HaierPacketToString(outData).c_str());
    //sendData(controlOutBuffer, controlOutBuffer[0]);
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
