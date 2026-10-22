#pragma once
#include <algorithm>
#include <cstdint>

// Heuristic AP pressure, not airtime utilization or RF noise measurement.
struct ChannelSurvey {
    uint16_t counts[13] = {};
    int16_t strongest[13] = {};
    uint32_t scores[13] = {};
    uint32_t total = 0;
    void add(int channel, int rssi) {
        if (channel < 1 || channel > 13 || rssi >= 0 || rssi < -127) return;
        counts[channel - 1]++;
        if (strongest[channel - 1] == 0 || rssi > strongest[channel - 1]) strongest[channel - 1] = rssi;
        total++;
        int strength = std::clamp(rssi + 100, 1, 80);
        uint32_t weight = strength * strength;
        for (int candidate = 1; candidate <= 13; candidate++) {
            int distance = candidate > channel ? candidate - channel : channel - candidate;
            unsigned overlap = distance == 0 ? 4 : distance <= 2 ? 2 : distance <= 4 ? 1 : 0;
            scores[candidate - 1] += weight * overlap;
        }
    }
    uint8_t recommend(uint8_t current, uint16_t allowed = 0x1FFF) const {
        if (total == 0) return 0;
        uint8_t best = 0;
        for (uint8_t channel : {uint8_t(1), uint8_t(6), uint8_t(11)}) {
            if (!(allowed & (1u << (channel - 1)))) continue;
            if (best == 0 || scores[channel - 1] < scores[best - 1]) best = channel;
        }
        if (best == 0) return 0;
        // Retain the current candidate when differences are small (10%, min 16).
        if ((current == 1 || current == 6 || current == 11) && (allowed & (1u << (current - 1))) &&
            scores[current - 1] <= scores[best - 1] + std::max(uint32_t(16), scores[best - 1] / 10)) return current;
        return best;
    }
};
