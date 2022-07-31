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
    /*  1 */    uint8_t             reserved[6];                // 0x40 or 0x00 for first byte, 0x00 for rest 0x00
    /*  7 */    uint8_t             msg_type;                   // type of message
    /*  8 */    uint8_t             arguments[2];
};

struct HaierPacketControl
{
    // Control bytes starts here
    /* 10 */    uint8_t             set_point;                  // Target temperature with 16°C offset (0x00 = 16°C)
    /* 11 */    uint8_t             vertical_swing_mode:4;      // See enum VerticalSwingMode
                uint8_t             :0;                         // Probably unused
    /* 12 */    uint8_t             fan_mode:4;                 // See enum FanMode
                uint8_t             ac_mode:4;                  // See enum ConditioningMode
    /* 13 */    uint8_t             :8;                         // ??? windSpeed
    /* 14 */    uint8_t             away_mode:1;                // Away mode for 10°C
                uint8_t             display_enabled:1;          // If the display is on or off
                uint8_t             use_half_degree:1;          // Use half degree value
                uint8_t             intelligence_status:1;      // ??? Intelligence status
                uint8_t             pmv_status:1;               // ??? Pulse Motor Valve status
                uint8_t             use_fahrenheit:1;           // Use Fahrenheit instead of Celsius
                uint8_t             :1;                         // ??? energySavePeriod
                uint8_t             self_clean_56:1;            // Self cleaning (56°C Steri-Clean)
    /* 15 */    uint8_t             ac_power:1;                 // Is ac on or off
                uint8_t             health_mode:1;              // Health mode on or off
                uint8_t             electric_heating_status:1;  // Electric heating status
                uint8_t             fast_mode:1;                // Fast mode
                uint8_t             quiet_mode:1;               // Quiet mode
                uint8_t             sleep_mode:1;               // Sleep mode
                uint8_t             lock_remote:1;              // Disable remote
                uint8_t             disable_beeper:1;           // If 1 disables AC's command feedback beeper (need to be set on every control command)
    /* 16 */    uint8_t             target_humidity;            // ??? Probably target humidity with with 30% offset
    /* 17 */    uint8_t             horizontal_swing_mode:4;    // See enum HorizontalSwingMode
                uint8_t             :0;                         // ??? Human sensing status
    /* 18 */    uint8_t             :8;                         // ??? Probably unused
    /* 19 */    uint8_t             fresh_air_status:1;         // ??? Fresh air status
                uint8_t             humidification_status:1;    // ??? Humidification status
                uint8_t             pm2p5_cleaning_status:1;    // ??? PM2.5 cleaning status
                uint8_t             ch20_cleaning_status:1;     // ??? CH2O cleaning status
                uint8_t             self_cleaning_status:1;     // ??? Self cleaning status
                uint8_t             light_status:1;             // ??? Light status
                uint8_t             energy_saving_status:1;     // ??? Energy saving status
                uint8_t             change_filter:1;            // ??? Filter need replacement
};

struct HaierPacketSensors
{
    /* 20 */    uint8_t             room_temperature;           // 0.5 °C step
    /* 21 */    uint8_t             room_humidity;              // ???
    /* 22 */    uint8_t             outdoor_temperature;        // 0.5 °C step
    /* 23 */    uint8_t             :8;                         // unknown (acType, probably 1 byte)
    /* 24 */    uint8_t             :8;                         // probably capabilities mask (each field 1 bit): sensingResult, airQuality, pm2p5Level, errCode, ErrAckFlag, operationModeHK)
    /* 25 */    uint8_t             operation_source;           // who is controlling AC (1 - remote, 3 - ESP)
    // Following bytes should have next fields (16 bytes):
    //      totalCleaningTime       (probably in seconds 4 bytes or minutes 2 bytes)
    //      indoorPM2p5Value        (probably 2 bytes)
    //      outdoorPM2p5Value       (probably 2 bytes)
    //      ch2oValue               (probably 2 bytes)
    //      vocValue                (probably 2 bytes)
    //      co2Value                (probably 2 bytes)
    //      totalElectricityUsed    (probably 4 bytes)
    // My AC don't support them (always 0) so no way to figure out how they encoded
};

struct HaierStatus
{
    HaierPacketHeader   header;
    HaierPacketControl  control;
    HaierPacketSensors  sensors;
};

struct HaierControl
{
    HaierPacketHeader   header;
    HaierPacketControl  control;
};

#define CONTROL_PACKET_SIZE         (sizeof(HaierPacketHeader) + sizeof(HaierPacketControl))
#define HEADER_SIZE                 (sizeof(HaierPacketHeader))

#endif // HAIER_PACKET_H