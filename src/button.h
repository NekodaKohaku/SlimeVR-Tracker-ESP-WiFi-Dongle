#pragma once

#include <Arduino.h>
#include <cstdint>
#include <functional>
#include <vector>

class Button {
public:
    static Button &getInstance();
    void begin();
    void update();
    void onLongPress(std::function<void()> callback);
    void onMultiPress(std::function<void(size_t)> callback);

private:
    void attach();

    void initDebouncing(bool state);
    bool isButtonPressed();

    void invokeLongPressCallbacks();
    void invokeMultiPressCallbacks(size_t pressCount);

    static Button instance;

    std::vector<std::function<void()>> longPressCallbacks;
    std::vector<std::function<void(size_t)>> multiPressCallbacks;

    bool polling{false};
    bool lastButtonState = false;
    uint32_t lastButtonChangeMillis{0};
    uint32_t pressCount = 0;

    uint32_t circularBuffer{0};

    static constexpr float longPressSeconds{2.0f};
    static constexpr float multiPressMaxDelaySeconds{1.0f};

    friend void button_isr();
};
