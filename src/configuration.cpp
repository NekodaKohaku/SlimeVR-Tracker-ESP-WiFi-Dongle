#include "configuration.h"

#include <Preferences.h>
#include <cstring>
#include <esp_mac.h>
#include <esp_random.h>

namespace {

static_assert(sizeof(WifiDongleConfig::apSsid) - 1
              <= (WifiDongleConfig::appendMacSuffixToDefaultSsid ? 25 : 32),
              "Default SSID is too long for the optional MAC suffix");
static_assert(sizeof(WifiDongleConfig::apPassword) - 1 >= 8
              && sizeof(WifiDongleConfig::apPassword) - 1 <= 63,
              "Fallback WiFi password must be 8-63 bytes");
static_assert(WifiDongleConfig::randomDefaultPasswordLength >= 8
              && WifiDongleConfig::randomDefaultPasswordLength <= 63,
              "Random WiFi password must be 8-63 bytes");

constexpr char randomPasswordCharacters[] =
    "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
static_assert(sizeof(randomPasswordCharacters) - 1 == 32,
              "Random password character set must contain 32 characters");

void generateRandomPassword(char *output, size_t length) {
    uint8_t randomBytes[WifiDongleConfig::randomDefaultPasswordLength];
    esp_fill_random(randomBytes, length);
    for (size_t index = 0; index < length; index++) {
        output[index] = randomPasswordCharacters[randomBytes[index] & 0x1F];
    }
    output[length] = '\0';
}

} // namespace

Configuration &Configuration::getInstance() {
    return instance;
}

void Configuration::setup() {
    // WiFi settings use NVS and do not depend on the tracker filesystem.
    loadWifi();

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
    std::lock_guard<std::mutex> lock(trackerMutex);
    auto file = LittleFS.open(trackerMapPath, "r");
    if (!file) return 0;
    uint8_t record[7]; uint16_t mask = 0;
    while (file.read(record, sizeof(record)) == sizeof(record))
        if (record[6] < maxTrackers) mask |= (1u << record[6]);
    file.close();
    return static_cast<uint8_t>(__builtin_popcount(mask));
}

void Configuration::setSavedTrackerCount(uint8_t newValue) {
    Serial.printf("New saved trackers count: %d\n", newValue);
    auto file = LittleFS.open(savedTrackerCountPath, "w", true);
    if (!file || file.write(&newValue, sizeof(newValue)) != sizeof(newValue)) {
        Serial.println("Could not save tracker count");
    }
    file.close();
}

bool Configuration::getOrCreateTrackerId(const uint8_t mac[6], uint8_t &trackerId) {
    std::lock_guard<std::mutex> lock(trackerMutex);
    for (uint8_t id = 0; id < maxTrackers; id++)
        if ((retiredIds & (1u << id)) && memcmp(removedMacs[id], mac, 6) == 0) return false;
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

    uint16_t usedMask = retiredIds;
    uint16_t savedMask = 0;
    if (LittleFS.exists(trackerMapPath)) {
        auto file = LittleFS.open(trackerMapPath, "r");
        uint8_t storedMac[6];
        uint8_t storedId;
        while (file.read(storedMac, sizeof(storedMac)) == sizeof(storedMac)
               && file.read(&storedId, sizeof(storedId)) == sizeof(storedId)) {
            if (storedId < maxTrackers) {
                usedMask |= (1u << storedId);
                savedMask |= (1u << storedId);
            }
        }
        file.close();
    }

    for (uint8_t id = 0; id < maxTrackers; id++) {
        if ((usedMask & (1u << id)) != 0) {
            continue;
        }

        auto file = LittleFS.open(trackerMapPath, "a", true);
        if (!file) return false;
        bool written = file.write(mac, 6) == 6
                       && file.write(&id, sizeof(id)) == sizeof(id);
        file.close();
		if (!written) return false;

        trackerId = id;
        setSavedTrackerCount(__builtin_popcount(savedMask) + 1);
        return true;
    }

    return false;
}

void Configuration::resetTrackers() {
    std::lock_guard<std::mutex> lock(trackerMutex);
    if (LittleFS.exists(trackerMapPath) && !LittleFS.remove(trackerMapPath)) {
        Serial.println("Could not remove tracker map");
        return;
    }
    setSavedTrackerCount(0);
}

bool Configuration::removeTracker(uint8_t trackerId) {
    std::lock_guard<std::mutex> lock(trackerMutex);
    if (trackerId >= maxTrackers || !LittleFS.exists(trackerMapPath)) return false;
    constexpr char tempPath[] = "/trackerMap.tmp";
    auto input = LittleFS.open(trackerMapPath, "r");
    auto output = LittleFS.open(tempPath, "w");
    if (!input || !output) { input.close(); output.close(); return false; }
    uint8_t record[7], removedMac[6] = {}; bool found = false, ok = true; uint8_t remaining = 0;
    while (input.available()) {
        if (input.read(record, sizeof(record)) != sizeof(record)) { ok = false; break; }
        if (record[6] == trackerId) { memcpy(removedMac, record, 6); found = true; continue; }
        if (output.write(record, sizeof(record)) != sizeof(record)) { ok = false; break; }
        remaining++;
    }
    output.flush(); input.close(); output.close();
    if (!ok || !found || !LittleFS.rename(tempPath, trackerMapPath)) {
        LittleFS.remove(tempPath); return false;
    }
    // Do not reuse this HID identity while the host still has this session open.
    retiredIds |= (1u << trackerId);
    memcpy(removedMacs[trackerId], removedMac, 6);
    setSavedTrackerCount(remaining);
    return true;
}

void Configuration::printTrackers(Print &out) {
    std::lock_guard<std::mutex> lock(trackerMutex);
    if (!LittleFS.exists(trackerMapPath)) {
        out.println("No stored trackers.");
        return;
    }

    auto file = LittleFS.open(trackerMapPath, "r");
    if (!file) {
        out.println("Could not read stored trackers.");
        return;
    }
    uint8_t mac[6];
    uint8_t id;
    size_t count = 0;
    while (file.read(mac, sizeof(mac)) == sizeof(mac)
           && file.read(&id, sizeof(id)) == sizeof(id)) {
        out.printf("  %u: %02X:%02X:%02X:%02X:%02X:%02X\n",
                   id, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        count++;
    }
    file.close();
    if (count == 0) {
        out.println("No stored trackers.");
    }
}

void Configuration::loadDeviceDefaults() {
    uint8_t mac[6] = {0};
    bool hasMac = esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK;
    if (WifiDongleConfig::appendMacSuffixToDefaultSsid && hasMac) {
        snprintf(wifiSsid, sizeof(wifiSsid), "%s-%02X%02X%02X",
                 WifiDongleConfig::apSsid, mac[3], mac[4], mac[5]);
    } else {
        snprintf(wifiSsid, sizeof(wifiSsid), "%s", WifiDongleConfig::apSsid);
    }

    snprintf(wifiPassword, sizeof(wifiPassword), "%s",
             WifiDongleConfig::apPassword);
    if (WifiDongleConfig::generateRandomDefaultPassword) {
        Preferences identity;
        if (identity.begin(identityNamespace, false)) {
            String generated = identity.getString(generatedPasswordKey, "");
            if (generated.length() != WifiDongleConfig::randomDefaultPasswordLength) {
                char newPassword[WifiDongleConfig::randomDefaultPasswordLength + 1];
                generateRandomPassword(newPassword,
                                       WifiDongleConfig::randomDefaultPasswordLength);
                if (identity.putString(generatedPasswordKey, newPassword)
                    == WifiDongleConfig::randomDefaultPasswordLength) {
                    generated = newPassword;
                }
            }
            if (generated.length() == WifiDongleConfig::randomDefaultPasswordLength) {
                snprintf(wifiPassword, sizeof(wifiPassword), "%s",
                         generated.c_str());
            }
            identity.end();
        }
    }

    wifiChannel = WifiDongleConfig::apChannel;
    wifiOverride = false;
}

void Configuration::loadWifi() {
    loadDeviceDefaults();

    Preferences preferences;
    if (!preferences.begin(preferencesNamespace, true)) {
        return;
    }

    if (preferences.getBool(wifiConfiguredKey, false)) {
        String savedSsid = preferences.getString(wifiSsidKey, "");
        String savedPassword = preferences.getString(wifiPasswordKey, "");
        uint8_t savedChannel = preferences.getUChar(wifiChannelKey, WifiDongleConfig::apChannel);
        if (savedSsid.length() >= 1 && savedSsid.length() <= 32
            && savedPassword.length() >= 8 && savedPassword.length() <= 63
            && savedChannel <= 13) {
            snprintf(wifiSsid, sizeof(wifiSsid), "%s", savedSsid.c_str());
            snprintf(wifiPassword, sizeof(wifiPassword), "%s", savedPassword.c_str());
            wifiChannel = savedChannel;
            wifiOverride = true;
        } else {
            Serial.println("Stored WiFi settings are invalid, using defaults");
        }
    }
    preferences.end();
}

bool Configuration::saveWifi() {
    Preferences preferences;
    if (!preferences.begin(preferencesNamespace, false)) {
        return false;
    }
    bool ok = preferences.putString(wifiSsidKey, wifiSsid) > 0;
    ok = preferences.putString(wifiPasswordKey, wifiPassword) > 0 && ok;
    ok = preferences.putUChar(wifiChannelKey, wifiChannel) == 1 && ok;
    ok = preferences.putBool(wifiConfiguredKey, true) == 1 && ok;
    preferences.end();
    if (ok) {
        wifiOverride = true;
    }
    return ok;
}

const char *Configuration::getWifiSsid() const { return wifiSsid; }
const char *Configuration::getWifiPassword() const { return wifiPassword; }
uint8_t Configuration::getWifiChannel() const { return wifiChannel; }
bool Configuration::hasWifiOverride() const { return wifiOverride; }

bool Configuration::setWifiSsid(const char *ssid) {
    size_t length = strlen(ssid);
    if (length < 1 || length > 32) return false;
    char previous[sizeof(wifiSsid)];
    memcpy(previous, wifiSsid, sizeof(previous));
    snprintf(wifiSsid, sizeof(wifiSsid), "%s", ssid);
    if (saveWifi()) return true;
    memcpy(wifiSsid, previous, sizeof(wifiSsid));
    return false;
}

bool Configuration::setWifiPassword(const char *password) {
    size_t length = strlen(password);
    if (length < 8 || length > 63) return false;
    char previous[sizeof(wifiPassword)];
    memcpy(previous, wifiPassword, sizeof(previous));
    snprintf(wifiPassword, sizeof(wifiPassword), "%s", password);
    if (saveWifi()) return true;
    memcpy(wifiPassword, previous, sizeof(wifiPassword));
    return false;
}

bool Configuration::setWifiChannel(uint8_t channel) {
    if (channel > 13) return false;
    uint8_t previous = wifiChannel;
    wifiChannel = channel;
    if (saveWifi()) return true;
    wifiChannel = previous;
    return false;
}

bool Configuration::resetWifi() {
    Preferences preferences;
    if (!preferences.begin(preferencesNamespace, false)) return false;
    bool cleared = preferences.clear();
    preferences.end();
    if (!cleared) return false;
    loadDeviceDefaults();
    return true;
}

Configuration Configuration::instance;
