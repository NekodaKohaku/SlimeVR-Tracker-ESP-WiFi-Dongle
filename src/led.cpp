#include "led.h"

#include "error_codes.h"
#include "pins_arduino.h"

#include <Arduino.h>

void LED::begin() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, !LED_ACTIVE_LEVEL);
}

void LED::update() {
    if (currentBlinkCount == 0 && !continuousBlinks) {
        return;
    }

    auto elapsedMillis = millis() - lastLedChangeMillis;
    auto waitSeconds =
            currentLedState ? currentBlinkOnSeconds : currentBlinkOffSeconds;

    if (elapsedMillis < waitSeconds * 1e3) {
        return;
    }

    lastLedChangeMillis += waitSeconds * 1e3;
    setState(!currentLedState);

    if (!currentLedState && currentBlinkCount > 0) {
        currentBlinkCount--;

        if (currentBlinkCount == 0 && continuousBlinks) {
            currentBlinkOnSeconds = currentContinuousBlinkOnSeconds;
            currentBlinkOffSeconds = currentContinuousBlinkOffSeconds;
        }
    }
}

void LED::setState(bool on) {
    digitalWrite(LED_BUILTIN, on == LED_ACTIVE_LEVEL);
    currentLedState = on;
}

// 一律閃固定 kErrorCodeBits 個位元,由最高位開始,長閃=1、短閃=0。
//
// 舊做法是「把錯誤碼反轉後一路右移到 0 為止」,等於自動去掉前導 0:
//   0x01 → 閃 1 個長                     ┐
//   0x02 → 反轉成 0b01,閃 1 個長          ├ 不同的錯誤閃出一模一樣的樣式
//   0x04 → 反轉成 0b001,閃 1 個長         ┘
// 而錯誤碼存在的唯一理由就是「沒有序列埠時也能分辨」。
// 固定位元數就沒有這個問題:AP_START_FAILED(0x01)=短短長、
// UDP_LISTEN_FAILED(0x02)=短長短。
static constexpr uint8_t kErrorCodeBits = 3;

// 新增錯誤碼超過 7 時,這裡會編譯失敗 —— 提醒把 kErrorCodeBits 一起加大,
// 否則新錯誤碼的高位會被靜默切掉、又變回「兩個錯誤閃同一個樣式」。
// 檢查的是哨兵 ERROR_CODE_COUNT(=最大錯誤碼+1)而不是某個具名錯誤碼:
// 盯著具名的那個,只要有人在它後面補一項,這道防線就靜默失效了。
static_assert(static_cast<unsigned>(ErrorCodes::ERROR_CODE_COUNT) <= (1u << kErrorCodeBits),
              "kErrorCodeBits too small for the largest ErrorCodes value");

void LED::blinkErrorOnce(uint8_t code) {
    for (int i = kErrorCodeBits - 1; i >= 0; i--) {
        setState(true);
        delay(((code >> i) & 0b1) ? 500 : 100);
        setState(false);
        delay(200);
    }
    delay(1000 - 200);
}

// 原本還有一個 [[noreturn]] 的 displayError():永遠閃下去、不返回。
// 它已經沒有任何呼叫端 —— 開機失敗改成閃三輪後回到 loop() 重試,
// 因為卡在無窮迴圈裡的話按鈕、USB HID、重試全都停擺,使用者只能拔電源。
// 留著一個「會讓韌體再也回不來」的公開函式只會誘導未來的人重新用它,故刪除。

void LED::displayErrorTimes(ErrorCodes errorCode, uint8_t repeats) {
    uint8_t code = static_cast<uint8_t>(errorCode);
    for (uint8_t i = 0; i < repeats; i++) {
        blinkErrorOnce(code);
    }
    setState(false);
}

void LED::sendBlinks(uint8_t blinkCount, float onSeconds, float offSeconds) {
    currentBlinkCount = blinkCount;
    currentBlinkOnSeconds = onSeconds;
    currentBlinkOffSeconds = offSeconds > 0 ? offSeconds : onSeconds;

    if (currentLedState) {
        lastLedChangeMillis = millis();
    } else {
        lastLedChangeMillis = millis() - currentBlinkOffSeconds * 1e3;
    }
    setState(false);
}

void LED::sendContinuousBlinks(float onSeconds, float offSeconds) {
    sendBlinks(0, onSeconds, offSeconds);
    continuousBlinks = true;
    currentContinuousBlinkOnSeconds = onSeconds;
    currentContinuousBlinkOffSeconds = offSeconds > 0 ? offSeconds : onSeconds;
}

void LED::stopBlinking() {
    currentBlinkCount = 0;
    continuousBlinks = false;
    setState(false);
}
