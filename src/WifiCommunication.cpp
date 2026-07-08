#include "WifiCommunication.h"

#include "configuration.h"
#include "packetHandling.h"
#include "espnow/messages.h"

#include "esp_wifi.h"

#define MACSTR        "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2ARGS(mac) mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]

volatile int8_t g_lastPacketRssi = 0;

WifiCommunication &WifiCommunication::getInstance() { return instance; }

ErrorCodes WifiCommunication::begin(const char *ssid, const char *pass,
                                    uint8_t channel, uint8_t maxConn, bool hidden) {
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(ssid, pass, channel, hidden ? 1 : 0, maxConn)) {
        Serial.println("Couldn't start SoftAP!");
        return ErrorCodes::ESP_NOW_INIT_FAILED;
    }

    wifi_config_t cfg;
    esp_wifi_get_config(WIFI_IF_AP, &cfg);
    cfg.ap.beacon_interval = WifiDongleConfig::beaconIntervalMs;
    cfg.ap.dtim_period     = WifiDongleConfig::dtimPeriod;
    esp_wifi_set_config(WIFI_IF_AP, &cfg);

    esp_wifi_set_ps(WIFI_PS_NONE);

    if (!udp.listen(udpPort)) {
        Serial.println("Couldn't start UDP listener!");
        return ErrorCodes::ESP_RECV_CALLACK_REGISTERING_FAILED;
    }
    udp.onPacket([](AsyncUDPPacket pkt) {
        WifiCommunication::getInstance().handleMessage(pkt);
    });

    Serial.printf("SoftAP \"%s\" up, channel %d, UDP %d\n", ssid, channel, udpPort);
    return ErrorCodes::NO_ERROR;
}

void WifiCommunication::enterPairingMode() { pairing = true; }
void WifiCommunication::exitPairingMode()  { pairing = false; }
bool WifiCommunication::isInPairingMode()  { return pairing; }

void WifiCommunication::onTrackerPaired(std::function<void()> cb) {
    trackerPairedCallbacks.push_back(cb);
}
void WifiCommunication::onTrackerConnected(std::function<void(uint8_t, const uint8_t *)> cb) {
    trackerConnectedCallbacks.push_back(cb);
}
void WifiCommunication::onPacketReceived(std::function<void(const uint8_t[packetSizeBytes])> cb) {
    packetReceivedCallbacks.push_back(cb);
}
void WifiCommunication::onInfoReceived(std::function<void(const uint8_t *)> cb) {
    infoReceivedCallbacks.push_back(cb);
}

void WifiCommunication::invokeTrackerPairedEvent() {
    for (auto &cb : trackerPairedCallbacks) cb();
}
void WifiCommunication::invokeTrackerConnectedEvent(uint8_t id, const uint8_t *mac) {
    for (auto &cb : trackerConnectedCallbacks) cb(id, mac);
}
void WifiCommunication::invokePacketReceivedEvent(const uint8_t *d) {
    for (auto &cb : packetReceivedCallbacks) cb(d);
}
void WifiCommunication::invokeInfoReceivedEvent(const uint8_t *d) {
    for (auto &cb : infoReceivedCallbacks) cb(d);
}

void WifiCommunication::handleMessage(AsyncUDPPacket &pkt) {
    const uint8_t *data = pkt.data();
    size_t len = pkt.length();
    if (len < 1) return;

    auto header = static_cast<ESPNowMessageHeader>(data[0]);
    switch (header) {
    case ESPNowMessageHeader::Pairing: {
        if (!pairing) return;
        if (len < sizeof(ESPNowPairingMessage)) return;

        auto *message = reinterpret_cast<const ESPNowPairingMessage *>(data);
        uint8_t nextId = 0;
        if (!Configuration::getInstance().getOrCreateTrackerId(message->mac, nextId)) {
            Serial.println("Pairing rejected: tracker table full");
            return;
        }

        ESPNowPairingAckMessage ack;
        ack.trackerId = nextId;
        pkt.write(reinterpret_cast<uint8_t *>(&ack), sizeof(ack));
        Serial.printf("Paired tracker " MACSTR " as id %d\n", MAC2ARGS(message->mac), nextId);
        invokeTrackerPairedEvent();
        break;
    }
    case ESPNowMessageHeader::PairingAck:
        return;
    case ESPNowMessageHeader::Connection: {
        if (len < 8) return;
        uint8_t trackerId = data[1];
        const uint8_t *mac = &data[2];

        if (trackerId < kMaxPeers) {
            m_peerIPs[trackerId] = pkt.remoteIP();
            m_peerPorts[trackerId] = pkt.remotePort();
            if (trackerId >= m_peerCount) m_peerCount = trackerId + 1;
        }

        uint8_t ack[2] = {
            static_cast<uint8_t>(ESPNowMessageHeader::Connection), trackerId
        };
        pkt.write(ack, sizeof(ack));
        Serial.printf("Tracker " MACSTR " connected as id %d\n",
                      MAC2ARGS(mac), trackerId);
        invokeTrackerConnectedEvent(trackerId, mac);
        break;
    }
    case ESPNowMessageHeader::Packet: {
        if (len < 1 + 15) return;
        invokePacketReceivedEvent(&data[1]);
        break;
    }
    case ESPNowMessageHeader::Info: {
        if (len < 1 + 14) return;
        invokeInfoReceivedEvent(&data[1]);
        break;
    }
    }
}

void WifiCommunication::sendExperimentHeartbeats() {
    if (!kExperimentHeartbeat) return;
    if (millis() - m_lastHeartbeatMs < kHeartbeatIntervalMs) return;
    m_lastHeartbeatMs = millis();

    uint8_t hb[12] = {0};
    hb[0] = 0xFE;
    for (size_t i = 0; i < m_peerCount; i++) {
        if (m_peerPorts[i] == 0) continue;
        udp.writeTo(hb, sizeof(hb), m_peerIPs[i], m_peerPorts[i]);
    }
}

void WifiCommunication::update() {
    sendExperimentHeartbeats();

    if (millis() - lastRssiMs < 1000) return;
    lastRssiMs = millis();

    wifi_sta_list_t list;
    if (esp_wifi_ap_get_sta_list(&list) != ESP_OK) return;

    PacketHandling &ph = PacketHandling::getInstance();
    for (int i = 0; i < list.num; i++) {
        ph.updateRssiByMac(list.sta[i].mac, list.sta[i].rssi);
    }
}

WifiCommunication WifiCommunication::instance;
