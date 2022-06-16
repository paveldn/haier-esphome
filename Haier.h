#ifndef HAIER_ESP_HAIER_H
#define HAIER_ESP_HAIER_H

#include "esphome.h"
#include <chrono>
#include <sstream>
#include <mutex>

#define MAX_MESSAGE_SIZE            64      //64 should be enough to cover largest messages we send and receive, for now. 

class HaierClimate :    public esphome::climate::Climate, 
                        public esphome::PollingComponent, 
                        public esphome::uart::UARTDevice 
{
public:
    HaierClimate() = delete;
	HaierClimate(const HaierClimate&) = delete;
    HaierClimate& operator=(const HaierClimate&) = delete;
    HaierClimate(esphome::uart::UARTComponent* parent);
	~HaierClimate();
    void setup() override;
    void loop() override;
    void update() override;
    void control(const ClimateCall &call) override;
protected:
    ClimateTraits traits() override;
    void sendData(const uint8_t * message, size_t size);
	void processStatus(const uint8_t* packet, uint8_t size);
private:
    bool        		mIsFirstStatusReceived;
	SemaphoreHandle_t	mReadMutex;
	uint8_t				mLastPacket[MAX_MESSAGE_SIZE];
	uint8_t				mFanModeFanSpeed;
	uint8_t				mOtherModesFanSpeed;
};


using namespace esphome;
using namespace esphome::climate;
using namespace esphome::uart;

#include <string>

#define HEADER                      0xFF
//message types seen and used
#define SEND_TYPE_POLL              0x73    // Next message is poll, enables command and poll messages to be replied to
#define RESPONSE_TYPE_POLL          0x74    // Message was received

#define SEND_POLL                   0x01    // Send a poll
#define RESPONSE_POLL               0x02    // Response of poll
#define ERROR_POLL                  0x03    // The ac sends this when it didnt like our last command message

#define SEND_TYPE_WIFI              0xFC    // Next message is wifi, enables wifi messages to be replied to, for the signal strength indicator, disables poll messages
#define RESPONSE_TYPE_WIFI          0xFD    // Confirmed
#define SEND_WIFI                   0xF7    // Current signal strength, no reply to this


#define SEND_COMMAND                0x60    // Send a control command, no reply to this
#define INIT_COMMAND                0x61    // This enables coms? magic message
#define CONTROL_PACKET_SIZE			0x14

#define POLY                        0xa001  //used for crc
#define NO_MSK                      0xFF
#define PACKET_TIMOUT_MS			1000

// temperatures supported by AC system
#define MIN_SET_TEMPERATURE         16
#define MAX_SET_TEMPERATURE         30

enum VerticalSwingMode
{
    VerticalSwingHealthUp       = 0x01,
    VerticalSwingMaxUp          = 0x02,
    VerticalSwingHealthDown     = 0x03,
    VerticalSwingUp             = 0x04,
    VerticalSwingCenter         = 0x06,
    VerticalSwingDown           = 0x08,
    VerticalSwingAuto           = 0x0C
};

enum HorizontalSwingMode
{
    HorizontalSwingCenter       = 0x00,
    HorizontalSwingMaxLeft      = 0x03,
    HorizontalSwingLeft         = 0x04,
    HorizontalSwingRight        = 0x05,
    HorizontalSwingMaxRight     = 0x06,
    HorizontalSwingAuto         = 0x07 
};

enum ConditioningMode
{
    ConditioningAuto            = 0x00,
    ConditioningDry             = 0x04,
    ConditioningCool            = 0x02,
    ConditioningHeat            = 0x08,
    ConditioningFan             = 0x0C
};

enum FanMode
{
    FanHigh                     = 0x01,
    FanMid                      = 0x02,
    FanLow                      = 0x03,
    FanAuto                     = 0x05
};

// Dead bits immediatly revert, 
// unknown bits stay on until the remote is used which turns them off
struct HaierPacket
{
	// We skip header 0xFF 0xFF
    /*  0 */    uint8_t             msg_length;                 // message length
    /*  1 */    uint8_t             reserved_1[6];              // 0x40 (0x00 for init) 0x00 0x00 0x00 0x00
    /*  7 */    uint8_t             msg_type;                   // type of message
    /*  8 */    uint8_t             msg_command;   
    /*  9 */    uint8_t             unknown_1;   
    // Control bytes starts here    
    /* 10 */    uint8_t             set_point;                  // 0x00 is 16 degrees C, offset of 16c for the set point
    /* 11 */    uint8_t             vertical_swing_mode:4;      // See enum VerticalSwingMode
                uint8_t             unused_1:4; 
    /* 12 */    uint8_t             fan_mode:4;                 // See enum FanMode
                uint8_t             ac_mode:4;                  // See enum ConditioningMode
    /* 13 */    uint8_t             unknown_2;  
    /* 14 */    uint8_t             away_mode:1;                //away mode for 10c
                uint8_t             display_enabled:1;          // if the display is on or off
                uint8_t             dead_1:3;   
                uint8_t             use_fahrenheit:1;           // use fahrenheit instead of celsius
                uint8_t             dead_2:1;   
                uint8_t             self_clean_56:1;            // Self cleaning (56°C Steri-Clean)
    /* 15 */    uint8_t             ac_power:1;                 // Is ac on or off
                uint8_t             purify_1:1;                 // Purify mode (can be related to bits of byte 21)
                uint8_t             unknown_3:1;    
                uint8_t             fast_mode:1;                // Fast mode
                uint8_t             quiet_mode:1;               // Quiet mode
                uint8_t             sleep_mode:1;               // Sleep mode
                uint8_t             lock_remote:1;              // Disable remote
                uint8_t             dead_3:1;
    /* 16 */    uint8_t             unknown_4:6;
                uint8_t             dead_4:2;
    /* 17 */    uint8_t             horizontal_swing_mode:4;    // See enum HorizontalSwingMode
                uint8_t             unused_2:4;
    /* 18 */    uint8_t             unknown_5;
    /* 19 */    uint8_t             purify_2:1;                 // Purify mode
                uint8_t             dead_5:1;
                uint8_t             purify_3:2;                 // Purify mode
                uint8_t             self_clean:1;               // Self cleaning (not 56°C)
                uint8_t             dead_6:3;
    /* 20 */    uint8_t             room_temperature;           // 0.5 °C step
    /* 21 */    uint8_t             unknown_6;                  // always 0
    /* 22 */    uint8_t             outdoor_temperature;        // 0.5 °C step
    /* 23 */    uint8_t             unknown_7[2];
    /* 25 */    uint8_t             compressor;                 //seeems to be 1 in off, 3 in heat? changeover valve?
				HaierPacket() : msg_length(0), msg_type(0)
				{}
};

std::string HaierPacketToString(const HaierPacket packet)
{
	std::stringstream sstream;
	sstream << "Message type: " << (uint)packet.msg_type << "\n"
			<< "\tSetpoint: " << (uint)packet.set_point + 16 << "\n"
			<< "\tAC mode: " << (uint)packet.ac_mode << "\n"
			<< "\tFan mode: " << (uint)packet.fan_mode << "\n"
			<< "\tVertical swing mode: " << (uint)packet.vertical_swing_mode << "\n"
			<< "\tHorizontal swing mode: " << (uint)packet.horizontal_swing_mode << "\n"
			<< "\tRoom temperature: " << (uint)packet.room_temperature / 2 << "\n"
			<< "\tOutdoor temperature: " << (uint)packet.room_temperature / 2 << "\n"
			<< "\tIs on?: " << ((packet.ac_power != 0) ? "Yes" : "No") << std::endl;
	return sstream.str();
}

uint8_t getChecksum(const uint8_t * message, size_t size) {
        uint8_t result = 0;
        for (int i = 0; i < size; i++){
            result += message[i];
        }
        return result;
    }

uint16_t crc16(uint16_t crc, const uint8_t *buf, size_t len)
{ 
    while (len--) {
        crc ^= *buf++;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
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

uint8_t initialization_1[15]    = {0x0A,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	INIT_COMMAND,	0x00,	0x07}; 					//enables coms? magic message, needs some timing pause after this?
uint8_t type_poll[12]           = {0x08,	0x40,	0x00,	0x00,	0x00,	0x00,	0x00,	SEND_TYPE_POLL}; 								//sets message type, we only use the poll type for now
uint8_t type_wifi[12]           = {0x08,	0x40,	0x00,	0x00,	0x00,	0x00,	0x00,	SEND_TYPE_WIFI};
uint8_t wifi_status[17]         = {0x0C,	0x40,	0x00,	0x00,	0x00,	0x00,	0x00,	SEND_WIFI,		0x00,	0x00,	0x00,	0x32}; 	//last byte is signal strength, second to last goes to 1 when trying to connect, not used for now
uint8_t poll_command[15]        = {0x0A,	0x40,	0x00,	0x00,	0x00,	0x00,	0x00,	SEND_POLL,		0x4D,	0x01};					//ask for the status


HaierClimate::HaierClimate(UARTComponent* parent) :   
                                        PollingComponent(5 * 1000), 
                                        UARTDevice(parent) ,
                                        mIsFirstStatusReceived(false),
										mFanModeFanSpeed(FanMid),
										mOtherModesFanSpeed(FanAuto)
{
	mReadMutex = xSemaphoreCreateMutex();
}

HaierClimate::~HaierClimate()
{
	vSemaphoreDelete(mReadMutex);
}

void HaierClimate::setup() 
{
	ESP_LOGW("Haier", "Running setup");
	delay(2000);                						//wait for the ac to boot, esp boots faster then the AC
    sendData(initialization_1, initialization_1[0]); 	//start the comms?
    sendData(type_poll, type_poll[0]);        			//enable polling and commands?
    delay(2000);                						//wait for the ac, if we dont wait it wont be ready, and it wont init. 
}

void HaierClimate::loop()  
{
	static uint8_t currentPacketBuffer[MAX_MESSAGE_SIZE];
	static uint8_t headerCounter = 0;
	static uint8_t currentPacketSize = 0;
	static uint8_t curentPacketPosition = 0;
	static std::chrono::steady_clock::time_point currentPacketTimeout;
	static bool validPacketFound = false;
	static uint8_t currentPacketChecksum = 0;
	#define ResetPacketRead() 	do { \
									validPacketFound = false; \
									headerCounter = 0; \
									curentPacketPosition = 0; \
									currentPacketSize = 0; \
									currentPacketChecksum = 0; \
								} while (0)
	
	if (validPacketFound && (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - currentPacketTimeout).count() > PACKET_TIMOUT_MS))
	{
		ESP_LOGW("Haier", "Incomming packet timeout, packet size %d, expected size %d", curentPacketPosition, currentPacketSize);
		ResetPacketRead();
	}
	while (available() > 0)
	{
		uint8_t val;
		if (!read_byte(&val))
			break;
		if (validPacketFound)
		{
			if (curentPacketPosition < currentPacketSize)
			{
				currentPacketBuffer[curentPacketPosition++] = val;
				if (curentPacketPosition < currentPacketSize - 2)
					currentPacketChecksum += val;
			}
			if (curentPacketPosition >= currentPacketSize)
			{
				bool isValid = true;
				if (currentPacketChecksum != currentPacketBuffer[currentPacketSize - 3])
				{
					ESP_LOGW("Haier", "Wrong packet checksum: 0x%02X (expected 0x%02X)", currentPacketChecksum, currentPacketBuffer[currentPacketSize - 3]);
					isValid = false;
				}
				else
				{
					uint16_t crc = crc16(0, currentPacketBuffer, currentPacketSize - 3);
					if (((crc >> 8) != currentPacketBuffer[currentPacketSize - 2]) || ((crc & 0xFF) != currentPacketBuffer[currentPacketSize - 1]))
					{
						ESP_LOGD("Haier", "Wrong packet checksum: 0x%04X (expected 0x%02X%02X)", crc, currentPacketBuffer[currentPacketSize - 2], currentPacketBuffer[currentPacketSize - 1]);
						isValid = false;
					}
				}
				if (isValid)
				{
					HaierPacket* pckt = reinterpret_cast<HaierPacket*>(currentPacketBuffer);
					std::string packet_type;
					switch (pckt->msg_type)
					{
						case RESPONSE_POLL:
							packet_type = "Status";
							xSemaphoreTake(mReadMutex, portMAX_DELAY);
							memcpy(mLastPacket, currentPacketBuffer, currentPacketSize);
							xSemaphoreGive(mReadMutex);
							processStatus(currentPacketBuffer, currentPacketSize);
							break;
						case RESPONSE_TYPE_WIFI:
							// Not supported yet
							packet_type = "Reponse to WiFi Type";
							break;
						case RESPONSE_TYPE_POLL:
							// Not supported yet
							packet_type = "Reponse to Poll Type";
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
					std::string raw = getHex(currentPacketBuffer, currentPacketSize);
					ESP_LOGD("Haier", "Received %s message, size: %d, content: %02X %02X%s", packet_type.c_str(), currentPacketSize, HEADER, HEADER, raw.c_str());
				}
				ResetPacketRead();
			}
		}
		else 
		{
			if (val == HEADER)
				headerCounter++;
			else
			{
				if (headerCounter >= 2)
				{
					if ((val + 3 <= MAX_MESSAGE_SIZE) and (val >= 10)) // Packet size should be at least 10
					{
						// Valid packet size
						currentPacketSize = val + 3;	// Space for checksum
						currentPacketBuffer[0] = val;
						currentPacketChecksum += val;
						curentPacketPosition = 1;
						currentPacketTimeout = std::chrono::steady_clock::now();
						validPacketFound = true;
					}
					else
						ESP_LOGW("Haier", "Wrong packet size %d", val);
				}
				headerCounter = 0;
			}
		}
	}
	#undef ResetPacketRead
}

void HaierClimate::update() 
{
    ESP_LOGD("Haier", "POLLING");
    if (!mIsFirstStatusReceived) {
        //if we havent gotten our first status
        ESP_LOGD("Haier", "First Status Not Received");
    }
    sendData(poll_command, poll_command[0]);
}

void HaierClimate::sendData(const uint8_t * message, size_t size)
{
	uint8_t crc = getChecksum(message, size);
	uint16_t crc_16 = crc16(0, message, size);
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
	HaierPacket& outData = (HaierPacket&) controlOutBuffer;
	outData.msg_type = 1;
	outData.msg_command = SEND_COMMAND;
	outData.msg_length = CONTROL_PACKET_SIZE;
    if (call.get_mode().has_value())
	{  
        switch (*call.get_mode()) 
		{
            case CLIMATE_MODE_OFF:
				outData.ac_power = 0;
                break;
                
            case CLIMATE_MODE_AUTO:
				outData.ac_power = 1;
				outData.ac_mode = ConditioningAuto;
				outData.fan_mode = mOtherModesFanSpeed;
                break;
                
            case CLIMATE_MODE_HEAT: 
				outData.ac_power = 1;
				outData.ac_mode = ConditioningHeat;
				outData.fan_mode = mOtherModesFanSpeed;
                break;
                
            case CLIMATE_MODE_DRY:   
				outData.ac_power = 1;
				outData.ac_mode = ConditioningDry;
				outData.fan_mode = mOtherModesFanSpeed;			               
                break;
                
            case CLIMATE_MODE_FAN_ONLY:
				outData.ac_power = 1;
				outData.ac_mode = ConditioningFan;
				outData.fan_mode = mFanModeFanSpeed;	// Auto doesnt work in fan only mode		               
                break;

            case CLIMATE_MODE_COOL:
				outData.ac_power = 1;
				outData.ac_mode = ConditioningCool;
				outData.fan_mode = mOtherModesFanSpeed;			               
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
				outData.fan_mode = FanLow;
                break;
            case CLIMATE_FAN_MEDIUM:
				outData.fan_mode = FanMid;
                break;
            case CLIMATE_FAN_HIGH:
                outData.fan_mode = FanHigh;
                break;
            case CLIMATE_FAN_AUTO:
                if (mode != CLIMATE_MODE_FAN_ONLY) //if we are not in fan only mode
                    outData.fan_mode = FanAuto;
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
				outData.horizontal_swing_mode = HorizontalSwingCenter;
				outData.vertical_swing_mode = VerticalSwingCenter;
                break;
            case CLIMATE_SWING_VERTICAL:
				outData.horizontal_swing_mode = HorizontalSwingCenter;
				outData.vertical_swing_mode = VerticalSwingAuto;
                break;
            case CLIMATE_SWING_HORIZONTAL:
				outData.horizontal_swing_mode = HorizontalSwingAuto;
				outData.vertical_swing_mode = VerticalSwingCenter;
                break;
            case CLIMATE_SWING_BOTH:
				outData.horizontal_swing_mode = HorizontalSwingAuto;
				outData.vertical_swing_mode = VerticalSwingAuto;
                break;
        }
    }
    if (call.get_target_temperature().has_value())
		outData.set_point = *call.get_target_temperature() - 16; //set the tempature at our offset, subtract 16.
    if (call.get_preset().has_value())
	{
        switch(call.get_preset().value()) 
		{
            case CLIMATE_PRESET_NONE:
				outData.away_mode 	= 0;
				outData.quiet_mode 	= 0;
				outData.fast_mode 	= 0;
				outData.sleep_mode 	= 0;
                break;
            case CLIMATE_PRESET_ECO:
				outData.away_mode 	= 0;
				outData.quiet_mode 	= 1;
				outData.fast_mode 	= 0;
				outData.sleep_mode 	= 0;
                break;
            case CLIMATE_PRESET_BOOST:
				outData.away_mode 	= 0;
				outData.quiet_mode 	= 0;
				outData.fast_mode 	= 1;
				outData.sleep_mode 	= 0;
                break;
            case CLIMATE_PRESET_AWAY:
				outData.away_mode 	= 1;
				outData.quiet_mode 	= 0;
				outData.fast_mode 	= 0;
				outData.sleep_mode 	= 0;
                break;
            case CLIMATE_PRESET_SLEEP:
				outData.away_mode 	= 0;
				outData.quiet_mode 	= 0;
				outData.fast_mode 	= 0;
				outData.sleep_mode 	= 1;
                break;
            default:
				ESP_LOGE("Control", "Unsupported preset");
                return;
        }
    }
	//ESP_LOGD("Control", HaierPacketToString(outData).c_str());
    sendData(controlOutBuffer, controlOutBuffer[0]);
}

void HaierClimate::processStatus(const uint8_t* packetBuffer, uint8_t size) 
{
	HaierPacket* packet = (HaierPacket*) packetBuffer;
	ESP_LOGD("Debug", "HVAC Mode = 0x%X", packet->ac_mode);
	ESP_LOGD("Debug", "Fan speed Status = 0x%X", packet->fan_mode);
	ESP_LOGD("Debug", "Horizontal Swing Status = 0x%X", packet->horizontal_swing_mode);
	ESP_LOGD("Debug", "Vertical Swing Status = 0x%X", packet->vertical_swing_mode);
	ESP_LOGD("Debug", "Set Point Status = 0x%X", packet->set_point);
	//extra modes/presets
	if (packet->quiet_mode != 0)
		   preset = CLIMATE_PRESET_ECO;
	else if (packet->fast_mode != 0)
		preset = CLIMATE_PRESET_BOOST;
	else if (packet->sleep_mode != 0)
		preset = CLIMATE_PRESET_SLEEP;
	else if(packet->away_mode != 0)
		preset = CLIMATE_PRESET_AWAY;
	else
		preset = CLIMATE_PRESET_NONE;   

	if (preset == CLIMATE_PRESET_AWAY) 
		target_temperature = 10;	//away mode sets to 10c
	else 
		target_temperature = packet->set_point + 16;
	current_temperature = packet->room_temperature / 2;	
	//remember the fan speed we last had for climate vs fan
	if (packet->ac_mode ==  ConditioningFan)
		mFanModeFanSpeed = packet->fan_mode;
	else
		mOtherModesFanSpeed = packet->fan_mode;
	switch (packet->fan_mode) 
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
	if (packet->ac_power == 0)
		mode = CLIMATE_MODE_OFF;
	else 
	{
		// Check current hvac mode
		switch (packet->ac_mode) 
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
	if (packet->horizontal_swing_mode == HorizontalSwingAuto)
	{
		if (packet->vertical_swing_mode == VerticalSwingAuto)
			swing_mode = CLIMATE_SWING_BOTH;
		else
			swing_mode = CLIMATE_SWING_HORIZONTAL;
	}
	else
	{
		if (packet->vertical_swing_mode == VerticalSwingAuto)
			swing_mode = CLIMATE_SWING_VERTICAL;
		else
			swing_mode = CLIMATE_SWING_OFF;
	}
	this->publish_state();
	mIsFirstStatusReceived = true;
}


#endif //HAIER_ESP_HAIER_H