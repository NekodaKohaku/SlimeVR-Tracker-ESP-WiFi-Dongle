#include "ChannelAnalysis.h"
#include "TrackerUdpReceiver.h"
#include "configuration.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <cstring>

ChannelAnalysis &ChannelAnalysis::getInstance() { static ChannelAnalysis instance; return instance; }
const char *ChannelAnalysis::start() {
    if (running) return "Error: scan already running.";
    if (WiFi.softAPgetStationNum() != 0 || TrackerUdpReceiver::getInstance().hasActiveTrackers())
        return "Error: turn off all trackers and disconnect WiFi clients before scanning.";
    survey = ChannelSurvey{}; recommended = 0; generation++; allowedMask = 0;
    originalChannel = WiFi.channel();
    wifi_country_t country{};
    if (esp_wifi_get_country(&country) == ESP_OK)
        for (unsigned channel = country.schan; channel < unsigned(country.schan) + country.nchan && channel <= 13; channel++)
            if (channel >= 1) allowedMask |= (1u << (channel - 1));
    if (!WiFi.mode(WIFI_AP_STA)) { state = "error"; finishedMs = millis(); return "Error: could not enable scan mode."; }
    startedMs = millis(); running = true; state = "running";
    // Async active AP discovery. The SDK controls the regulatory scan range.
    int count = WiFi.scanNetworks(true, true, false, 120);
    if (count == WIFI_SCAN_FAILED) { finish("error"); return "Error: could not start scan."; }
    if (count >= 0) collect(count);
    return nullptr;
}
void ChannelAnalysis::finish(const char *nextState) {
    esp_wifi_scan_stop();
    WiFi.scanDelete();
    running = false; state = nextState; finishedMs = millis();
    if (!WiFi.mode(WIFI_AP) || originalChannel < 1 || originalChannel > 13 ||
        esp_wifi_set_channel(originalChannel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        state = "error"; recommended = 0;
    }
}
void ChannelAnalysis::collect(int count) {
    uint8_t ownMac[6] = {}; WiFi.softAPmacAddress(ownMac);
    for (int i = 0; i < count; i++) {
        auto mac = WiFi.BSSID(i);
        if (!mac || memcmp(mac, ownMac, 6) == 0) continue;
        bool duplicate = false;
        for (int j = 0; j < i; j++) {
            auto prior = WiFi.BSSID(j);
            if (prior && memcmp(mac, prior, 6) == 0) { duplicate = true; break; }
        }
        if (!duplicate) survey.add(WiFi.channel(i), WiFi.RSSI(i));
    }
    recommended = survey.recommend(originalChannel, allowedMask);
    finish(survey.total == 0 ? "empty" : "ready");
}
void ChannelAnalysis::update() {
    if (!running) return;
    if (WiFi.softAPgetStationNum() != 0 || TrackerUdpReceiver::getInstance().hasActiveTrackers()) { finish("interrupted"); return; }
    if (uint32_t(millis() - startedMs) > 15000) { finish("error"); return; }
    int count = WiFi.scanComplete();
    if (count >= 0) collect(count);
    else if (count == WIFI_SCAN_FAILED) finish("error");
}
void ChannelAnalysis::cancel() { if (running) finish("cancelled"); }
bool ChannelAnalysis::saveRecommendation(uint32_t requestedGeneration, uint8_t channel) {
    if (running || strcmp(state, "ready") != 0 || requestedGeneration != generation ||
        uint32_t(millis() - finishedMs) > 300000 || channel != recommended ||
        !(channel == 1 || channel == 6 || channel == 11)) return false;
    return Configuration::getInstance().setWifiChannel(channel);
}
void ChannelAnalysis::printStatus(Print &out) {
    out.println("Channels v1");
    out.printf("state: %s\ngeneration: %u\nage_ms: %u\ncurrent: %u\nsaved: %u\nrecommended: %u\nap_total: %u\nallowed_mask: %u\n",
               state, generation, uint32_t(millis() - finishedMs), WiFi.channel(),
               Configuration::getInstance().getWifiChannel(), recommended, survey.total, allowedMask);
    if (!running && (strcmp(state, "ready") == 0 || strcmp(state, "empty") == 0))
        for (unsigned channel = 1; channel <= 13; channel++)
            out.printf("C,%u,%u,%d,%u\n", channel, survey.counts[channel - 1], survey.strongest[channel - 1], survey.scores[channel - 1]);
    out.println("Channels end");
}
