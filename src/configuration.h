#pragma once

#include "WifiDongleConfig.h"

#include <LittleFS.h>
#include <cstdint>

class Configuration {
public:
    static Configuration &getInstance();
    void setup();
    uint8_t getSavedTrackerCount();
    void setSavedTrackerCount(uint8_t newValue);
    bool getOrCreateTrackerId(const uint8_t mac[6], uint8_t &trackerId);
    void resetTrackers();

private:
    Configuration() = default;

    static Configuration instance;

    static constexpr char savedTrackerCountPath[] = "/savedTrackerCount.bin";
    static constexpr char trackerMapPath[] = "/trackerMap.bin";
    static constexpr uint8_t maxTrackers = WifiDongleConfig::maxTrackers;
};
