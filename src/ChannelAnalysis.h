#pragma once
#include <Arduino.h>
#include "ChannelSurvey.h"

class ChannelAnalysis {
public:
    static ChannelAnalysis &getInstance();
    const char *start();
    void update();
    void cancel();
    void printStatus(Print &out);
    bool saveRecommendation(uint32_t generation, uint8_t channel);
private:
    const char *state = "idle";
    ChannelSurvey survey;
    uint32_t generation = 0, startedMs = 0, finishedMs = 0;
    uint8_t recommended = 0, originalChannel = 0;
    uint16_t allowedMask = 0;
    bool running = false;
    void finish(const char *nextState);
    void collect(int count);
};
