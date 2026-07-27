#pragma once

#include "error_codes.h"

#include <cstdint>

class LED {
public:
    void begin();
    void update();
    void setState(bool on);
    // 把錯誤碼閃 repeats 輪就返回,讓呼叫端能接著重試/重開機。
    // (原本另有一個永不返回的 displayError();刪除理由見 led.cpp。)
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

    // 阻塞地把錯誤碼閃一輪(固定位元數,長閃=1、短閃=0)
    void blinkErrorOnce(uint8_t code);
};
