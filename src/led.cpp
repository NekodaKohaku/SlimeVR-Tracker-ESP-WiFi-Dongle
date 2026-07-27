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

static constexpr uint8_t kErrorCodeBits = 3;
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
