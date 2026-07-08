#include "configuration.h"

#include <cstring>

Configuration &Configuration::getInstance() {
    return instance;
}

void Configuration::setup() {
    bool status = LittleFS.begin();
    if (!status) {
        Serial.println("Could not mount LittleFS, formatting");

        status = LittleFS.format();
        if (!status) {
            Serial.println("Could not format LittleFS, aborting");
            return;
        }

        status = LittleFS.begin();
        if (!status) {
            Serial.println("Could not mount LittleFS, aborting");
            return;
        }
    }
    Serial.println("LittleFS is mounted");
}

uint8_t Configuration::getSavedTrackerCount() {
    if (!LittleFS.exists(savedTrackerCountPath)) {
        Serial.printf("%s doesn't exist, returning 0 saved trackers\n", savedTrackerCountPath);
        return 0;
    }

    auto file = LittleFS.open(savedTrackerCountPath, "r");
    uint8_t result;
    file.read(&result, sizeof(uint8_t));
    file.close();

    return result;
}

void Configuration::setSavedTrackerCount(uint8_t newValue) {
    Serial.printf("New saved trackers count: %d\n", newValue);
    auto file = LittleFS.open(savedTrackerCountPath, "w", true);
    file.write(&newValue, sizeof(uint8_t));
    file.close();
}

bool Configuration::getOrCreateTrackerId(const uint8_t mac[6], uint8_t &trackerId) {
    if (LittleFS.exists(trackerMapPath)) {
        auto file = LittleFS.open(trackerMapPath, "r");
        uint8_t storedMac[6];
        uint8_t storedId;
        while (file.read(storedMac, sizeof(storedMac)) == sizeof(storedMac)
               && file.read(&storedId, sizeof(storedId)) == sizeof(storedId)) {
            if (memcmp(storedMac, mac, sizeof(storedMac)) == 0) {
                trackerId = storedId;
                file.close();
                return trackerId < maxTrackers;
            }
        }
        file.close();
    }

    uint16_t usedMask = 0;
    if (LittleFS.exists(trackerMapPath)) {
        auto file = LittleFS.open(trackerMapPath, "r");
        uint8_t storedMac[6];
        uint8_t storedId;
        while (file.read(storedMac, sizeof(storedMac)) == sizeof(storedMac)
               && file.read(&storedId, sizeof(storedId)) == sizeof(storedId)) {
            if (storedId < maxTrackers) {
                usedMask |= (1u << storedId);
            }
        }
        file.close();
    }

    for (uint8_t id = 0; id < maxTrackers; id++) {
        if ((usedMask & (1u << id)) != 0) {
            continue;
        }

        auto file = LittleFS.open(trackerMapPath, "a", true);
        file.write(mac, 6);
        file.write(&id, sizeof(id));
        file.close();

        trackerId = id;
        setSavedTrackerCount(id + 1);
        return true;
    }

    return false;
}

void Configuration::resetTrackers() {
    setSavedTrackerCount(0);
    if (LittleFS.exists(trackerMapPath)) {
        LittleFS.remove(trackerMapPath);
    }
}

Configuration Configuration::instance;
