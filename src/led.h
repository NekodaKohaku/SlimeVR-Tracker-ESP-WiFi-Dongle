#pragma once

#include "error_codes.h"

#include <cstdint>

class LED {
public:
    void begin();
    void update();
    void setState(bool on);

    void displayErrorTimes(ErrorCodes errorCode, uint8_t repeats);
    void sendBlinks(uint8_t blinkCount, float onSeconds, float offSeconds = -1);
    void sendContinuousBlinks(float onSeconds, float offSeconds = -1);
    void stopBlinking();

private:
    uint8_t currentBlinkCount = 0;
    bool continuousBlinks = false;
    float currentBlinkOnSeconds = 0;
    float currentBlinkOffSeconds = 0;
    float currentContinuousBlinkOnSeconds = 0;
    float currentContinuousBlinkOffSeconds = 0;
    uint32_t lastLedChangeMillis;
    bool currentLedState = false;

    void blinkErrorOnce(uint8_t code);
};
