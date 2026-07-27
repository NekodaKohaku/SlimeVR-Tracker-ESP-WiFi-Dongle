#pragma once

#include "WifiDongleConfig.h"

#include <LittleFS.h>
#include <cstdint>

enum class TrackerIdStatus : uint8_t { Ok, Permanent, Transient };

class Configuration {
public:
    static Configuration &getInstance();
    void setup();
    uint8_t getSavedTrackerCount();
    void setSavedTrackerCount(uint8_t newValue);
    TrackerIdStatus getOrCreateTrackerId(const uint8_t mac[6], uint8_t &trackerId);
    void resetTrackers();

private:
    Configuration() = default;

    static Configuration instance;

    static constexpr char savedTrackerCountPath[] = "/savedTrackerCount.bin";
    static constexpr char trackerMapPath[] = "/trackerMap.bin";

    static constexpr char trackerMapTmpPath[] = "/trackerMap.tmp";
    static constexpr uint8_t maxTrackers = WifiDongleConfig::maxTrackers;

    static constexpr size_t mapRecordSize = 7;

    bool fsReady = false;

    struct RamEntry { uint8_t mac[6]; uint8_t id; bool used; };
    RamEntry ramMap[maxTrackers] = {};

    static constexpr uint8_t maxTransientFailures = 3;
    uint8_t transientFailures = 0;

    TrackerIdStatus getOrCreateTrackerIdRam(const uint8_t mac[6], uint8_t &trackerId);
    TrackerIdStatus getOrCreateTrackerIdFs(const uint8_t mac[6], uint8_t &trackerId);

    bool repairTrackerMapAlignment();

    void recoverPendingRepair();
};
