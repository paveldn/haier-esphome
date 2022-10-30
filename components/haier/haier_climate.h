#ifndef _HAIER_CLIMATE_H
#define _HAIER_CLIMATE_H

#include <chrono>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "haier_packet.h"

#if ESP8266
// No mutexes for ESP8266 just make dummy classes and pray...
struct Mutex
{
    void lock() {};
    void unlock() {};
};

struct Lock
{
    Lock(const Mutex&) {};
}; 
#else // USE_ESP32
#include <mutex>
typedef std::mutex Mutex;
typedef std::lock_guard<Mutex> Lock;
#endif

namespace esphome {
namespace haier {

enum AirflowVerticalDirection: size_t
{
    vdUp        = 0,
    vdCenter    = 1,
    vdDown      = 2,
};

enum AirflowHorizontalDirection: size_t
{
    hdLeft      = 0,
    hdCenter    = 1,
    hdRight     = 2,
};

class HaierClimate :    public esphome::Component,
                        public esphome::climate::Climate,
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
    void control(const esphome::climate::ClimateCall &call) override;
    void dump_config() override;
    float get_setup_priority() const override { return esphome::setup_priority::HARDWARE ; }
    void set_send_wifi_signal(bool sendWifi);
    void set_beeper_echo(bool beeper);
    bool get_beeper_echo() const;   
    void set_fahrenheit(bool fahrenheit);
    void set_outdoor_temperature_sensor(esphome::sensor::Sensor *sensor);
    void set_display_state(bool state);
    bool get_display_state() const;
    AirflowVerticalDirection get_vertical_airflow() const;
    void set_vertical_airflow(AirflowVerticalDirection direction);
    AirflowHorizontalDirection get_horizontal_airflow() const;
    void set_horizontal_airflow(AirflowHorizontalDirection direction);
    void set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes);
protected:
    esphome::climate::ClimateTraits traits() override;
    void sendFrame(HaierProtocol::FrameType type, const uint8_t* data=NULL, size_t data_size=0); 
    void sendFrameWithSubcommand(HaierProtocol::FrameType type, uint16_t subcommand, const uint8_t* data=NULL, size_t data_size=0);
    void sendData(const uint8_t * message, size_t size, bool withCrc = true);
    void processStatus(const uint8_t* packet, uint8_t size);
    void handleIncomingPacket();
    void getSerialData();
    void sendControlPacket(const esphome::climate::ClimateCall* control = NULL);
private:
    enum ProtocolPhases
    {
        // Initialization
        psSendingInit1 = 0,
        psWaitingAnswerInit1,
        psSendingInit2,
        psWaitingAnswerInit2,
        psSendingFirstStatusRequest,
        psWaitingFirstStatusAnswer,
        psSendingAlarmStatusReauest,
        psWaitingAlarmStatusAnswer,
        // Functional state
        psIdle,
        psSendingStatusRequest,
        psWaitingStatusAnswer,
        psSendingUpdateSignalRequest,
        psWaitingUpateSignalAnswer,
        psSendingSignalLevel,
        psWaitingSignalLevelAnswer,

    };
    void                        setPhase(ProtocolPhases phase);
    ProtocolPhases              mPhase;
    Mutex                       mReadMutex;
    uint8_t*                    mLastPacket;
    uint8_t                     mFanModeFanSpeed;
    uint8_t                     mOtherModesFanSpeed;
    bool                        mSendWifiSignal;
    bool                        mBeeperStatus;
    bool                        mUseFahrenheit;
    bool                        mDisplayStatus;
    bool                        mForceSendControl;
    AirflowVerticalDirection    mVerticalDirection;
    AirflowHorizontalDirection  mHorizontalDirection;
    bool                        mHvacHardwareInfoAvailable;
    std::string                 mHvacProtocolVersion;
    std::string                 mHvacSoftwareVersion;
    std::string                 mHvacHardwareVersion;
    std::string                 mHvacDeviceName;
    bool                        mHvacFunctions[5];
    bool&                       mUseCrc;
    uint8_t                     mActiveAlarms[8];
    esphome::sensor::Sensor*                mOutdoorSensor;
    esphome::climate::ClimateTraits         mTraits;
    std::chrono::steady_clock::time_point   mLastByteTimestamp;         // For packet timeout
    std::chrono::steady_clock::time_point   mLastRequestTimestamp;      // For answer timeout
    std::chrono::steady_clock::time_point   mLastValidStatusTimestamp;  // For protocol timeout
    std::chrono::steady_clock::time_point   mLastStatusRequest; // To request AC status
    std::chrono::steady_clock::time_point   mLastSignalRequest; // To send WiFI signal level

};

} // namespace haier
} // namespace esphome


#endif // _HAIER_CLIMATE_H