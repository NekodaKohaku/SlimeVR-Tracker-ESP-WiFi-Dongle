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

void LED::displayError(ErrorCodes errorCode) {
    uint8_t pattern = 0;
    uint8_t errorBits = static_cast<uint8_t>(errorCode);
    while (errorBits != 0) {
        pattern = pattern << 1 | (errorBits & 0b1);
        errorBits >>= 1;
    }

    while (true) {
        auto bitsLeft = pattern;

        while (bitsLeft != 0) {
            setState(true);
            delay(bitsLeft & 0b1 ? 500 : 100);
            setState(false);
            delay(200);

            bitsLeft >>= 1;
        }

        delay(1000 - 200);
    }
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
