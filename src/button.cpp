#include "button.h"

#include "pins_arduino.h"

void IRAM_ATTR button_isr() {
    Button &button = Button::getInstance();
    detachInterrupt(USER_BUTTON);
    button.initDebouncing(true);
    button.polling = true;
}

Button &Button::getInstance() {
    return instance;
}

void Button::begin() {
    pinMode(USER_BUTTON,
            USER_BUTTON_ACTIVE_LEVEL ? INPUT_PULLDOWN : INPUT_PULLUP);
    attach();
}

void Button::update() {
    if (!polling) {
        return;
    }

    auto buttonState = isButtonPressed();
    auto elapsedMillis = millis() - lastButtonChangeMillis;

    if (!buttonState && !lastButtonState) {
        if (elapsedMillis < multiPressMaxDelaySeconds * 1e3) {
            return;
        }

        if (pressCount > 1) {
            invokeMultiPressCallbacks(pressCount);
        }

        polling = false;
        attach();
        return;
    }

    if (buttonState && lastButtonState) {
        if (elapsedMillis < longPressSeconds * 1e3 || pressCount > 0) {
            return;
        }

        invokeLongPressCallbacks();
        polling = false;
        attach();
        return;
    }

    if (!buttonState) {
        pressCount++;
    }

    lastButtonState = buttonState;
    lastButtonChangeMillis = millis();
}

void Button::initDebouncing(bool state) {
    circularBuffer = state ? 0xffff : 0x0000;
    lastButtonState = state;
    lastButtonChangeMillis = millis();
    pressCount = 0;
}

bool Button::isButtonPressed() {
    circularBuffer = (circularBuffer << 1)
                     | (digitalRead(USER_BUTTON) == USER_BUTTON_ACTIVE_LEVEL);

    uint8_t popCount = __builtin_popcount(circularBuffer);

    return popCount > 16;
}

void Button::onLongPress(std::function<void()> callback) {
    longPressCallbacks.push_back(callback);
}

void Button::onMultiPress(std::function<void(size_t)> callback) {
    multiPressCallbacks.push_back(callback);
}

void Button::attach() {
    attachInterrupt(digitalPinToInterrupt(USER_BUTTON),
                    button_isr,
                    USER_BUTTON_ACTIVE_LEVEL ? RISING : FALLING);
}

void Button::invokeLongPressCallbacks() {
    for (auto &callback : longPressCallbacks) {
        callback();
    }
}

void Button::invokeMultiPressCallbacks(size_t pressCount) {
    for (auto &callback : multiPressCallbacks) {
        callback(pressCount);
    }
}

Button Button::instance;
