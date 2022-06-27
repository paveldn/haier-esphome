#ifndef HAIER_ESP_HAIER_CLIMATE_H
#define HAIER_ESP_HAIER_CLIMATE_H

#include "esphome.h"
#include <chrono>


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
protected:
    esphome::climate::ClimateTraits traits() override;
    void sendData(const uint8_t * message, size_t size, bool withCrc = true);
    void processStatus(const uint8_t* packet, uint8_t size);
    void handleIncomingPacket();
    void getSerialData();
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
        // Functional state
        psIdle,
        psSendingStatusRequest,
        psWaitingStatusAnswer,
        psSendingUpdateSignalRequest,
        psWaitingUpateSignalAnswer,
        psSendingSignalLevel,   // No answer to this command
    };
    ProtocolPhases      mPhase;
    SemaphoreHandle_t   mReadMutex;
    uint8_t*            mLastPacket;
    uint8_t             mFanModeFanSpeed;
    uint8_t             mOtherModesFanSpeed;
    esphome::climate::ClimateTraits         mTraits;
    std::chrono::steady_clock::time_point   mLastByteTimestamp;         // For packet timeout
    std::chrono::steady_clock::time_point   mLastRequestTimestamp;      // For answer timeout
    std::chrono::steady_clock::time_point   mLastValidStatusTimestamp;  // For protocol timeout
    std::chrono::steady_clock::time_point   mLastStatusRequest; // To request AC status
    std::chrono::steady_clock::time_point   mLastSignalRequest; // To send WiFI signal level

};

#endif //HAIER_ESP_HAIER_CLIMATE_H