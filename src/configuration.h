#pragma once

#include "WifiDongleConfig.h"

#include <LittleFS.h>
#include <Print.h>
#include <cstdint>

class Configuration {
public:
    static Configuration &getInstance();
    void setup();
    uint8_t getSavedTrackerCount();
    void setSavedTrackerCount(uint8_t newValue);
    bool getOrCreateTrackerId(const uint8_t mac[6], uint8_t &trackerId);
    void resetTrackers();
    void printTrackers(Print &out);

    const char *getWifiSsid() const;
    const char *getWifiPassword() const;
    uint8_t getWifiChannel() const;
    bool hasWifiOverride() const;
    bool setWifiSsid(const char *ssid);
    bool setWifiPassword(const char *password);
    bool setWifiChannel(uint8_t channel);
    bool resetWifi();

private:
    Configuration() = default;

    static Configuration instance;

    static constexpr char savedTrackerCountPath[] = "/savedTrackerCount.bin";
    static constexpr char trackerMapPath[] = "/trackerMap.bin";
    static constexpr uint8_t maxTrackers = WifiDongleConfig::maxTrackers;

    static constexpr char preferencesNamespace[] = "svrdongle";
    static constexpr char wifiConfiguredKey[] = "wifi_set";
    static constexpr char wifiSsidKey[] = "ssid";
    static constexpr char wifiPasswordKey[] = "password";
    static constexpr char wifiChannelKey[] = "channel";
    static constexpr char identityNamespace[] = "svridentity";
    static constexpr char generatedPasswordKey[] = "ap_pass";

    char wifiSsid[33] = {0};
    char wifiPassword[64] = {0};
    uint8_t wifiChannel = WifiDongleConfig::apChannel;
    bool wifiOverride = false;

    void loadWifi();
    void loadDeviceDefaults();
    bool saveWifi();
};
