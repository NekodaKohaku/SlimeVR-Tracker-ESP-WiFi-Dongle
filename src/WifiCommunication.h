#pragma once

#include "WifiDongleConfig.h"
#include "error_codes.h"

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <cstdint>
#include <functional>
#include <vector>

class WifiCommunication {
public:
    static constexpr size_t packetSizeBytes = 20;
    static constexpr uint16_t udpPort = WifiDongleConfig::udpPort;

    static WifiCommunication &getInstance();

    ErrorCodes begin(const char *ssid, const char *pass,
                     uint8_t channel, uint8_t maxConn, bool hidden);

    void enterPairingMode();
    void exitPairingMode();
    bool isInPairingMode();

    void onTrackerPaired(std::function<void()> callback);
    void onTrackerConnected(
            std::function<void(uint8_t, const uint8_t *)> callback);
    void onPacketReceived(
            std::function<void(const uint8_t data[packetSizeBytes])> callback);
    void onInfoReceived(std::function<void(const uint8_t *data)> callback);

    void update();

private:
    WifiCommunication() = default;
    static WifiCommunication instance;

    void handleMessage(AsyncUDPPacket &pkt);

    void invokeTrackerPairedEvent();
    void invokeTrackerConnectedEvent(uint8_t trackerId, const uint8_t *mac);
    void invokePacketReceivedEvent(const uint8_t *data);
    void invokeInfoReceivedEvent(const uint8_t *data);

    AsyncUDP udp;
    bool pairing = false;
    uint32_t lastRssiMs = 0;

    static constexpr bool kExperimentHeartbeat = WifiDongleConfig::customExperimentHeartbeat;
    static constexpr uint32_t kHeartbeatIntervalMs = WifiDongleConfig::customExperimentHeartbeatIntervalMs;
    static constexpr size_t kMaxPeers = WifiDongleConfig::maxTrackers;
    IPAddress m_peerIPs[kMaxPeers];
    uint16_t  m_peerPorts[kMaxPeers] = {0};
    size_t    m_peerCount = 0;
    uint32_t  m_lastHeartbeatMs = 0;
    void sendExperimentHeartbeats();

    std::vector<std::function<void()>> trackerPairedCallbacks;
    std::vector<std::function<void(uint8_t, const uint8_t *)>> trackerConnectedCallbacks;
    std::vector<std::function<void(const uint8_t *)>> packetReceivedCallbacks;
    std::vector<std::function<void(const uint8_t *)>> infoReceivedCallbacks;
};
