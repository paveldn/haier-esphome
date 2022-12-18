#ifndef _HAIER_CLIMATE_H
#define _HAIER_CLIMATE_H

#include <chrono>
#include <set>
#include "esphome.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
//#include "haier_packet.h"
#include "Transport/protocol_transport.h"
#include "Protocol/haier_protocol.h"

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


enum class FrameType : uint8_t
{
	ftControl                               = 0x01,     // Requests or sets one or multiple parameters (module <-> device, master-slave/interactive, required)
	ftStatus                                = 0x02,     // Contains one or multiple parameters values, usually answer to control frame (module <-> device, master-slave/interactive, required)
	ftInvalid                               = 0x03,     // Communication error indication (module <-> device, master-slave/interactive, required)
	ftAlarmStatus                           = 0x04,     // Alarm status report (module <-> device, interactive, required)
	ftConfirm                               = 0x05,     // Acknowledgment, usually used to confirm reception of frame if there is no special answer (module <-> device, master-slave/interactive, required)
	ftReport                                = 0x06,     // Report frame (module <-> device, interactive, required)
	ftStopFaultAlarm                        = 0x09,     // Stop fault alarm frame (module -> device, interactive, required)
	ftSystemDownlik                         = 0x11,     // System downlink frame (module -> device, master-slave/interactive, optional)
	ftDeviceUplink                          = 0x12,     // Device uplink frame (module <- device , interactive, optional)
	ftSystemQuery                           = 0x13,     // System query frame (module -> device, master-slave/interactive, optional)
	ftSystemQueryResponse                   = 0x14,     // System query response frame (module <- device , master-slave/interactive, optional)
	ftDeviceQuery                           = 0x15,     // Device query frame (module <- device, master-slave/interactive, optional)
	ftDeviceQueryResponse                   = 0x16,     // Device query response frame (module -> device, master-slave/interactive, optional)
	ftGroupCommand                          = 0x60,     // Group command frame (module -> device, interactive, optional)
	ftGetDeviceVersion                      = 0x61,     // Requests device version (module -> device, master-slave/interactive, required)
	ftGetDeviceVersionResponse              = 0x62,     // Device version answer (module <- device, master-slave/interactive, required_
	ftGetAllAddresses                       = 0x67,     // Requests all slave addresses (module -> device, interactive, optional)
	ftGetAllAddressesResponse               = 0x68,     // Answer to request of all slave addresses (module <- device , interactive, optional)
	ftHandsetChangeNotification             = 0x69,     // Handset change notification frame (module <- device , interactive, optional)
	ftGetDeviceId                           = 0x70,     // Requests Device ID (module -> device, master-slave/interactive, required)
	ftGetDeviceIdResponse                   = 0x71,     // Response to device ID request (module <- device , master-slave/interactive, required)
	ftGetAlarmStatus                        = 0x73,     // Alarm status request (module -> device, master-slave/interactive, required)
	ftGetAlarmStatusResponse                = 0x74,     // Response to alarm status request (module <- device, master-slave/interactive, required)
	ftGetDeviceConfiguration                = 0x7C,     // Requests device configuration (module -> device, interactive, required)
	ftGetDeviceConfigurationResponse        = 0x7D,     // Response to device configuration request (module <- device, interactive, required)
	ftDownlinkTransparentTransmission       = 0x8C,     // Downlink transparent transmission (proxy data Haier cloud -> device) (module -> device, interactive, optional)
	ftUplinkTransparentTransmission         = 0x8D,     // Uplink transparent transmission (proxy data device -> Haier cloud) (module <- device, interactive, optional)
	ftStartDeviceUpgrade                    = 0xE1,     // Initiate device OTA upgrade (module -> device, master-slave/interactive, OTA required)
	ftStartDeviceUpgradeResponse            = 0xE2,     // Response to initiate device upgrade command (module <- device, master-slave/interactive, OTA required)
	ftGetFirmwareContent                    = 0xE5,     // Requests to send firmware (module <- device, master-slave/interactive, OTA required)
	ftGetFirmwareContentResponse            = 0xE6,     // Response to send firmware request (module -> device, master-slave/interactive, OTA required) (multipacket?)
	ftChangeBaudRate                        = 0xE7,     // Requests to change port baud rate (module <- device, master-slave/interactive, OTA required)
	ftChangeBaudRateResponse                = 0xE8,     // Response to change port baud rate request (module -> device, master-slave/interactive, OTA required)
	ftGetSubboardInfo                       = 0xE9,     // Requests subboard information (module -> device, master-slave/interactive, required)
	ftGetSubboardInfoResponse               = 0xEA,     // Response to subboard information request (module <- device, master-slave/interactive, required)
	ftGetHardwareInfo                       = 0xEB,     // Requests information about device and subboard (module -> device, master-slave/interactive, required)
	ftGetHardwareInfoResponse               = 0xEC,     // Response to hardware information request (module <- device, master-slave/interactive, required)
	ftGetUpgradeResult                      = 0xED,     // Requests result of the firmware update (module <- device, master-slave/interactive, OTA required)
	ftGetUpgradeResultResponse              = 0xEF,     // Response to firmware update results request (module -> device, master-slave/interactive, OTA required)
	ftGetNetworkStatus                      = 0xF0,     // Requests network status (module <- device, interactive, optional)
	ftGetNetworkStatusResponse              = 0xF1,     // Response to network status request (module -> device, interactive, optional)
	ftStartWifiConfiguration                = 0xF2,     // Starts WiFi configuration procedure (module <- device, interactive, required)
	ftStartWifiConfigurationResponse        = 0xF3,     // Response to start WiFi configuration request (module -> device, interactive, required)
	ftStopWifiConfiguration                 = 0xF4,     // Stop WiFi configuration procedure (module <- device, interactive, required)
	ftStopWifiConfigurationResponse         = 0xF5,     // Response to stop WiFi configuration request (module -> device, interactive, required)
	ftReportNetworkStatus                   = 0xF7,     // Reports network status (module -> device, master-slave/interactive, required)
	ftClearConfiguration                    = 0xF8,     // Request to clear module configuration (module <- device, interactive, optional)
	ftBigDataReportConfiguration            = 0xFA,     // Configuration for autoreport device full status (module -> device, interactive, optional)
	ftBigDataReportConfigurationResponse    = 0xFB,     // Response to set big data configuration (module <- device, interactive, optional)
	ftGetManagementInformation              = 0xFC,     // Request management information from device (module -> device, master-slave, required)
	ftGetManagementInformationResponse      = 0xFD,     // Response to management information request (module <- device, master-slave, required)
	ftWakeUp                                = 0xFE,     // Request to wake up (module <-> device, master-slave/interactive, optional)
};

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
						public esphome::uart::UARTDevice,
						public HaierProtocol::ProtocolStream
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
    void set_beeper_state(bool state);
    bool get_beeper_state() const;   
    void set_fahrenheit(bool fahrenheit);
    void set_outdoor_temperature_sensor(esphome::sensor::Sensor *sensor);
    void set_display_state(bool state);
    bool get_display_state() const;
    AirflowVerticalDirection get_vertical_airflow() const;
    void set_vertical_airflow(AirflowVerticalDirection direction);
    AirflowHorizontalDirection get_horizontal_airflow() const;
    void set_horizontal_airflow(AirflowHorizontalDirection direction);
    void set_supported_swing_modes(const std::set<esphome::climate::ClimateSwingMode> &modes);
	virtual size_t available() noexcept { return esphome::uart::UARTDevice::available(); };
    virtual size_t read_array(uint8_t* data, size_t len) noexcept { return esphome::uart::UARTDevice::read_array(data, len) ? len : 0; };
    virtual void write_array(const uint8_t* data, size_t len) noexcept { esphome::uart::UARTDevice::write_array(data, len);};
	bool canSendMessage() const { return mHaierProtocol.getOutgoingQueueSize() == 0; };
protected:
    enum ProtocolPhases
    {
        psUnknown = -1,
        // Initialization
/* 0*/  psSendingInit1 = 0,
/* 1*/  psWaitingAnswerInit1,
/* 2*/  psSendingInit2,
/* 3*/  psWaitingAnswerInit2,
/* 4*/  psSendingFirstStatusRequest,
/* 5*/  psWaitingFirstStatusAnswer,
/* 6*/  psSendingAlarmStatusRequest,
/* 7*/  psWaitingAlarmStatusAnswer,
        // Functional state
/* 8*/  psIdle,
/* 9*/  psSendingStatusRequest,
/*10*/  psWaitingStatusAnswer,
/*11*/  psSendingUpdateSignalRequest,
/*12*/  psWaitingUpateSignalAnswer,
/*13*/  psSendingSignalLevel,
/*14*/  psWaitingSignalLevelAnswer,
/*15*/  psSendingControl,
/*16*/  psWaitingControlAnswer,
    };
    esphome::climate::ClimateTraits traits() override;
    // Answers handlers
    HaierProtocol::HandlerError answer_preprocess(uint8_t requestMessageType, uint8_t expectedRequestMessageType, uint8_t answerMessageType, uint8_t expectedAnswerMessageType, ProtocolPhases expectedPhase);
	HaierProtocol::HandlerError process_GetDeviceVersionAnswer(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
	HaierProtocol::HandlerError process_GetDeviceIdAnswer(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
    HaierProtocol::HandlerError process_Status(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
    HaierProtocol::HandlerError process_GetManagementInformationAnswer(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
    HaierProtocol::HandlerError process_ReportNetworkStatusAnswer(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
    HaierProtocol::HandlerError process_GetAlarmStatusAnswer(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
    // Timeout handler
    HaierProtocol::HandlerError handler_timeout(uint8_t requestType);
    HaierProtocol::HandlerError processStatusMessage(const uint8_t* packet, uint8_t size);
    void sendMessage(const HaierProtocol::HaierMessage& command);
    const HaierProtocol::HaierMessage getControlMessage();
private:
	HaierProtocol::ProtocolHandler		mHaierProtocol;
    struct HvacSettings
    {
        esphome::optional<esphome::climate::ClimateMode>		mode;
        esphome::optional<esphome::climate::ClimateFanMode>		fan_mode;
        esphome::optional<esphome::climate::ClimateSwingMode>	swing_mode;
        esphome::optional<float>				                target_temperature;
        esphome::optional<esphome::climate::ClimatePreset> 		preset;
        bool                                                    valid;
        HvacSettings() : valid(false) {};
        void reset();
    };
    void                        setPhase(ProtocolPhases phase);
    ProtocolPhases              mPhase;
    Mutex                       mConfigMUtex;
    Mutex                       mControlMutex;
    uint8_t*                    mLastStatusMessage;
    uint8_t                     mFanModeFanSpeed;
    uint8_t                     mOtherModesFanSpeed;
    bool                        mSendWifiSignal;
    bool                        mBeeperStatus;
    bool                        mUseFahrenheit;
    bool                        mDisplayStatus;
    bool                        mForceSendControl;
    bool                        mForcedPublish;
    bool                        mForcedRequestStatus;
    bool                        mControlCalled;
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
    // Use locks with this variable!
    HvacSettings                            mHvacSettings;
    std::chrono::steady_clock::time_point   mLastRequestTimestamp;      // For interval between messages
    std::chrono::steady_clock::time_point   mLastValidStatusTimestamp;  // For protocol timeout
    std::chrono::steady_clock::time_point   mLastStatusRequest;         // To request AC status
    std::chrono::steady_clock::time_point   mLastSignalRequest;         // To send WiFI signal level
    std::chrono::steady_clock::time_point   mControlRequestTimestamp;   // To send control message

};

} // namespace haier
} // namespace esphome


#endif // _HAIER_CLIMATE_H