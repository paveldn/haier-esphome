#ifndef HAIER_PACKET_H
#define HAIER_PACKET_H

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

struct HaierPacketHeader
{
    // We skip start packet indication (0xFF 0xFF)
    /*  0 */    uint8_t             msg_length;                 // message length
    /*  1 */    uint8_t             reserved[6];				// 0x40 or 0x00 for first byte, 0x00 for rest 0x00
    /*  7 */    uint8_t             msg_type;                   // type of message
    /*  8 */    uint8_t             arguments[2];
};

struct HaierPacketControl
{
    // Control bytes starts here
    /* 10 */    uint8_t             set_point;                  // 0x00 is 16 degrees C, offset of 16c for the set point
    /* 11 */    uint8_t             vertical_swing_mode:4;      // See enum VerticalSwingMode
                uint8_t             unused_1:4;
    /* 12 */    uint8_t             fan_mode:4;                 // See enum FanMode
                uint8_t             ac_mode:4;                  // See enum ConditioningMode
    /* 13 */    uint8_t             unknown_1;
    /* 14 */    uint8_t             away_mode:1;                //away mode for 10c
                uint8_t             display_enabled:1;          // if the display is on or off
                uint8_t             unknown_2:3;
                uint8_t             use_fahrenheit:1;           // use Fahrenheit instead of Celsius
                uint8_t             unknown_3:1;
                uint8_t             self_clean_56:1;            // Self cleaning (56°C Steri-Clean)
    /* 15 */    uint8_t             ac_power:1;                 // Is ac on or off
                uint8_t             purify_1:1;                 // Purify mode (can be related to bits of byte 21)
                uint8_t             unknown_4:1;
                uint8_t             fast_mode:1;                // Fast mode
                uint8_t             quiet_mode:1;               // Quiet mode
                uint8_t             sleep_mode:1;               // Sleep mode
                uint8_t             lock_remote:1;              // Disable remote
                uint8_t             unknown_5:1;
    /* 16 */    uint8_t             unknown_6;
    /* 17 */    uint8_t             horizontal_swing_mode:4;    // See enum HorizontalSwingMode
                uint8_t             unused_2:4;
    /* 18 */    uint8_t             unknown_7;
    /* 19 */    uint8_t             purify_2:1;                 // Purify mode
                uint8_t             unknown_8:1;
                uint8_t             purify_3:2;                 // Purify mode
                uint8_t             self_clean:1;               // Self cleaning (not 56°C)
                uint8_t             unknown_9:3;
};

struct HaierPacketSensors
{
    /* 20 */    uint8_t             room_temperature;           // 0.5 °C step
    /* 21 */    uint8_t             unknown_1;                  // always 0 (Room humidity for devices that support it ?)
    /* 22 */    uint8_t             outdoor_temperature;        // 0.5 °C step
    /* 23 */    uint8_t             unknown_2[2];				// unknown (Outdoor humidity for devices that support it + something else ?)
    /* 25 */    uint8_t             compressor;                 // seems to be 1 in off, 3 in heat? changeover valve?
};

struct HaierStatus
{
	HaierPacketHeader	header;
	HaierPacketControl	control;
	HaierPacketSensors	sensors;
};

struct HaierControl
{
	HaierPacketHeader	header;
	HaierPacketControl	control;
};

#define CONTROL_PACKET_SIZE         (sizeof(HaierPacketHeader) + sizeof(HaierPacketControl))
#define HEADER_SIZE 				(sizeof(HaierPacketHeader))

#endif // HAIER_PACKET_H