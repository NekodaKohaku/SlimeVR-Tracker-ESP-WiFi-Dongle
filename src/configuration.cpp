#include "configuration.h"

#include <cstring>

Configuration &Configuration::getInstance() {
    return instance;
}

void Configuration::setup() {
    fsReady = false;

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
    fsReady = true;
    transientFailures = 0;
    recoverPendingRepair();
    Serial.println("LittleFS is mounted");
}

uint8_t Configuration::getSavedTrackerCount() {
    if (!fsReady) return 0;
    if (!LittleFS.exists(savedTrackerCountPath)) {
        Serial.printf("%s doesn't exist, returning 0 saved trackers\n", savedTrackerCountPath);
        return 0;
    }

    auto file = LittleFS.open(savedTrackerCountPath, "r");
    if (!file) return 0;
    uint8_t result = 0;
    if (file.read(&result, sizeof(uint8_t)) != sizeof(uint8_t)) result = 0;
    file.close();

    return result;
}

void Configuration::setSavedTrackerCount(uint8_t newValue) {
    if (!fsReady) return;
    Serial.printf("New saved trackers count: %d\n", newValue);
    auto file = LittleFS.open(savedTrackerCountPath, "w", true);
    if (!file) {
        Serial.println("Could not open savedTrackerCount for write");
        return;
    }
    file.write(&newValue, sizeof(uint8_t));
    file.close();
}

TrackerIdStatus Configuration::getOrCreateTrackerIdRam(const uint8_t mac[6], uint8_t &trackerId) {
    for (uint8_t i = 0; i < maxTrackers; i++) {
        if (ramMap[i].used && memcmp(ramMap[i].mac, mac, 6) == 0) {
            trackerId = ramMap[i].id;
            return TrackerIdStatus::Ok;
        }
    }
    for (uint8_t i = 0; i < maxTrackers; i++) {
        if (ramMap[i].used) continue;
        memcpy(ramMap[i].mac, mac, 6);
        ramMap[i].id = i;
        ramMap[i].used = true;
        trackerId = i;
        return TrackerIdStatus::Ok;
    }
    return TrackerIdStatus::Permanent;
}

bool Configuration::repairTrackerMapAlignment() {
    uint8_t buf[maxTrackers * mapRecordSize];
    size_t keep = 0;
    size_t sz = 0;
    {
        auto file = LittleFS.open(trackerMapPath, "r");
        if (!file) return false;
        sz = file.size();
        keep = sz - (sz % mapRecordSize);

        if (keep > sizeof(buf)) keep = sizeof(buf);
        if (keep > 0 && file.read(buf, keep) != keep) {
            file.close();
            return false;
        }
        file.close();
    }
    Serial.printf("trackerMap misaligned (%u bytes), keeping %u\n",
                  static_cast<unsigned>(sz), static_cast<unsigned>(keep));

    if (keep == 0) {

        return LittleFS.remove(trackerMapPath);
    }

    auto out = LittleFS.open(trackerMapTmpPath, "w", true);
    if (!out) return false;
    bool ok = (out.write(buf, keep) == keep);
    out.close();
    if (!ok) {
        LittleFS.remove(trackerMapTmpPath);
        return false;
    }

    if (!LittleFS.remove(trackerMapPath)) {
        LittleFS.remove(trackerMapTmpPath);
        return false;
    }
    if (!LittleFS.rename(trackerMapTmpPath, trackerMapPath)) {

        Serial.println("trackerMap rename FAILED (data is in the temp file)");
        return false;
    }
    return true;
}

void Configuration::recoverPendingRepair() {
    if (!LittleFS.exists(trackerMapTmpPath)) return;
    if (LittleFS.exists(trackerMapPath)) {

        LittleFS.remove(trackerMapTmpPath);
        return;
    }
    Serial.println("recovering trackerMap from interrupted repair");
    LittleFS.rename(trackerMapTmpPath, trackerMapPath);
}

TrackerIdStatus Configuration::getOrCreateTrackerId(const uint8_t mac[6], uint8_t &trackerId) {
    if (!fsReady) return getOrCreateTrackerIdRam(mac, trackerId);

    TrackerIdStatus st = getOrCreateTrackerIdFs(mac, trackerId);
    if (st == TrackerIdStatus::Transient) {
        if (++transientFailures >= maxTransientFailures) {
            Serial.println("trackerMap storage keeps failing, falling back to RAM pairing");
            fsReady = false;
            return getOrCreateTrackerIdRam(mac, trackerId);
        }
    } else {
        transientFailures = 0;
    }
    return st;
}

TrackerIdStatus Configuration::getOrCreateTrackerIdFs(const uint8_t mac[6], uint8_t &trackerId) {

    uint16_t usedMask = 0;
    if (LittleFS.exists(trackerMapPath)) {
        {

            auto probe = LittleFS.open(trackerMapPath, "r");
            if (!probe) return TrackerIdStatus::Transient;
            size_t sz = probe.size();
            bool aligned = (sz % mapRecordSize == 0);
            probe.close();
            if (!aligned && !repairTrackerMapAlignment()) {
                Serial.println("trackerMap repair FAILED");
                return TrackerIdStatus::Transient;
            }
        }
        auto file = LittleFS.open(trackerMapPath, "r");
        if (file) {
            uint8_t rec[mapRecordSize];
            while (file.read(rec, mapRecordSize) == mapRecordSize) {
                const uint8_t *storedMac = rec;
                uint8_t storedId = rec[6];
                if (memcmp(storedMac, mac, 6) == 0) {
                    trackerId = storedId;
                    file.close();

                    return storedId < maxTrackers ? TrackerIdStatus::Ok
                                                  : TrackerIdStatus::Permanent;
                }
                if (storedId < maxTrackers) {
                    if (usedMask & (1u << storedId)) {

                        Serial.printf("trackerMap has duplicate id %u (table is stale)\n",
                                      static_cast<unsigned>(storedId));
                    }
                    usedMask |= static_cast<uint16_t>(1u << storedId);
                }
            }
            file.close();
        }
    }

    for (uint8_t id = 0; id < maxTrackers; id++) {
        if ((usedMask & (1u << id)) != 0) {
            continue;
        }

        uint8_t rec[mapRecordSize];
        memcpy(rec, mac, 6);
        rec[6] = id;

        auto file = LittleFS.open(trackerMapPath, "a", true);
        if (!file) {
            Serial.println("Could not open trackerMap for append");
            return TrackerIdStatus::Transient;
        }
        bool ok = (file.write(rec, mapRecordSize) == mapRecordSize);
        file.close();
        if (!ok) {
            Serial.println("Could not write trackerMap entry");
            return TrackerIdStatus::Transient;
        }

        trackerId = id;

        return TrackerIdStatus::Ok;
    }

    return TrackerIdStatus::Permanent;
}

void Configuration::resetTrackers() {

    memset(ramMap, 0, sizeof(ramMap));
    if (!fsReady) return;
    setSavedTrackerCount(0);
    if (LittleFS.exists(trackerMapPath)) {
        LittleFS.remove(trackerMapPath);
    }
    if (LittleFS.exists(trackerMapTmpPath)) {
        LittleFS.remove(trackerMapTmpPath);
    }
    transientFailures = 0;
}

Configuration Configuration::instance;
