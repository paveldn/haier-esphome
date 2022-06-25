#ifndef HAIER_PACKET_H
#define HAIER_PACKET_H

#define MAX_MESSAGE_SIZE            64      //64 should be enough to cover largest messages we send and receive, for now.

#define HEADER                      0xFF
//message types seen and used
#define SEND_TYPE_POLL              0x73    // Next message is poll, enables command and poll messages to be replied to
#define RESPONSE_TYPE_POLL          0x74    // Message was received

// Incomming packet types (from AC point of view)
#define SEND_POLL                   0x01    // Send a poll
#define SEND_COMMAND                0x60    // Send a control command, no reply to this
#define INIT_COMMAND                0x61    // This enables coms? magic message
#define SEND_WIFI                   0xF7    // Current signal strength, no reply to this

// Responses (from AC point of view)
#define RESPONSE_POLL               0x02    // Response of poll
#define ERROR_POLL                  0x03    // The ac sends this when it didnt like our last command message

#define RESPONSE_TYPE_WIFI          0xFD    // Confirmed

#define SEND_TYPE_WIFI              0xFC 	//next message is wifi, enables wifi messages to be replied to, for the signal strength indicator, disables poll messages
#define SEND_POLL                   0x01	//send a poll


#define CONTROL_PACKET_SIZE         0x14

#define POLY                        0xa001  //used for crc
#define NO_MSK                      0xFF
#define PACKET_TIMOUT_MS            1000

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
struct HaierPacketHeader
{
    // We skip start packet indication (0xFF 0xFF)
    /*  0 */    uint8_t             msg_length;                 // message length
    /*  1 */    uint8_t             reserved[6];				// 0x40 (0x00 for init) 0x00 0x00 0x00 0x00
    /*  7 */    uint8_t             msg_type;                   // type of message
    /*  8 */    uint8_t             msg_command;
    /*  9 */    uint8_t             unknown;
};

struct HaierPacketControl
{
    // Control bytes starts here
    /* 10 */    uint8_t             set_point;                  // 0x00 is 16 degrees C, offset of 16c for the set point
    /* 11 */    uint8_t             vertical_swing_mode:4;      // See enum VerticalSwingMode
                uint8_t             unused_1:4;
    /* 12 */    uint8_t             fan_mode:4;                 // See enum FanMode
                uint8_t             ac_mode:4;                  // See enum ConditioningMode
    /* 13 */    uint8_t             unknown;
    /* 14 */    uint8_t             away_mode:1;                //away mode for 10c
                uint8_t             display_enabled:1;          // if the display is on or off
                uint8_t             dead_1:3;
                uint8_t             use_fahrenheit:1;           // use Fahrenheit instead of Celsius
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
};

struct HaierPacketSensors
{
    /* 20 */    uint8_t             room_temperature;           // 0.5 °C step
    /* 21 */    uint8_t             unknown_6;                  // always 0
    /* 22 */    uint8_t             outdoor_temperature;        // 0.5 °C step
    /* 23 */    uint8_t             unknown_7[2];
    /* 25 */    uint8_t             compressor;                 //seems to be 1 in off, 3 in heat? changeover valve?
};

struct HaierPacketFull
{
	HaierPacketHeader	header;
	HaierPacketControl	control;
	HaierPacketSensors	sensors;
};

#endif // HAIER_PACKET_H