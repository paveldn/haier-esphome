#ifndef HAIER_ESP_HAIER_CLIMATE_H
#define HAIER_ESP_HAIER_CLIMATE_H

#include "esphome.h"


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
    void control(const esphome::climate::ClimateCall &call) override;
protected:
    esphome::climate::ClimateTraits traits() override;
    void sendData(const uint8_t * message, size_t size);
    void processStatus(const uint8_t* packet, uint8_t size);
private:
    bool                mIsFirstStatusReceived;
    SemaphoreHandle_t   mReadMutex;
    uint8_t*            mLastPacket;
    uint8_t             mFanModeFanSpeed;
    uint8_t             mOtherModesFanSpeed;
};

#endif //HAIER_ESP_HAIER_CLIMATE_H