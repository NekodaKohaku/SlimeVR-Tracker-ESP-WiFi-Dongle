

#include "TrackerUdpReceiver.h"
#include "packetHandling.h"
#include "configuration.h"
#include "esp_wifi.h"
#include "ChannelSurvey.h"

TrackerUdpReceiver TrackerUdpReceiver::instance;

static const char kHandshakeMagic[] = "Hey OVR =D 5";
static constexpr size_t kHandshakeMagicLen = 12;

TrackerUdpReceiver &TrackerUdpReceiver::getInstance() { return instance; }

ErrorCodes TrackerUdpReceiver::begin(
	const char *ssid, const char *password,
	uint8_t channel, uint8_t maxConn, bool hidden
) {
	uint8_t useChannel = channel;
	if (channel == 0) {
		useChannel = pickBestChannel();
	}

	WiFi.mode(WIFI_AP);
	if (!WiFi.softAP(ssid, password, useChannel, hidden ? 1 : 0, maxConn)) {
		Serial.println("[Receiver] Could not start SoftAP");
		return ErrorCodes::AP_START_FAILED;
	}

	wifi_config_t cfg;
	esp_wifi_get_config(WIFI_IF_AP, &cfg);
	cfg.ap.beacon_interval = kBeaconIntervalMs;
	cfg.ap.dtim_period     = kDtimPeriod;
	esp_wifi_set_config(WIFI_IF_AP, &cfg);
	esp_wifi_set_ps(WIFI_PS_NONE);

	if (!m_udp.listen(kPort)) {
		Serial.printf("[Receiver] Could not listen on UDP port %u\n", kPort);
		return ErrorCodes::UDP_LISTEN_FAILED;
	}
	m_udp.onPacket([this](AsyncUDPPacket pkt) { onPacket(pkt); });
	Serial.printf("[Receiver] SoftAP on channel %u, listening on UDP %u\n", useChannel, kPort);
	return ErrorCodes::NO_ERROR;
}

uint8_t TrackerUdpReceiver::pickBestChannel() {
    WiFi.mode(WIFI_AP_STA);
    int count = WiFi.scanNetworks(false, true);
    ChannelSurvey survey;
    for (int i = 0; i < count; i++) survey.add(WiFi.channel(i), WiFi.RSSI(i));
    WiFi.scanDelete();
    uint8_t best = survey.recommend(1);
    return best == 0 ? 1 : best;
}

bool TrackerUdpReceiver::hasActiveTrackers() {
    std::lock_guard<std::mutex> lock(m_peerMutex);
    uint32_t now = millis();
    for (const auto &peer : m_peers)
        if (peer.used && !peer.removed && peer.connected && uint32_t(now - peer.lastSeenMs) < kTrackerTimeoutMs) return true;
    return false;
}

bool TrackerUdpReceiver::parseHandshake(const uint8_t *data, size_t len, uint8_t outMac[6],
                                    uint32_t &boardType, uint32_t &mcuType, uint32_t &imuType) {
	constexpr size_t kBodyOffset = 12;
	constexpr size_t kFwLenOffset = 40;
	if (len <= kFwLenOffset) return false;

	boardType = readBeU32(&data[kBodyOffset]);
	imuType   = readBeU32(&data[kBodyOffset + 4]);
	mcuType   = readBeU32(&data[kBodyOffset + 8]);

	uint8_t fwLen = data[kFwLenOffset];
	size_t macOffset = kFwLenOffset + 1 + fwLen;
	if (len < macOffset + 6) return false;
	memcpy(outMac, &data[macOffset], 6);
	return true;
}

int TrackerUdpReceiver::findPeerByMac(const uint8_t mac[6]) {
	for (size_t i = 0; i < kMaxTrackers; i++)
		if (m_peers[i].used && memcmp(m_peers[i].mac, mac, 6) == 0)
			return static_cast<int>(i);
	return -1;
}

int TrackerUdpReceiver::findPeerByEndpoint(const IPAddress &ip, uint16_t port) {
	for (size_t i = 0; i < kMaxTrackers; i++)
		if (m_peers[i].used && m_peers[i].endpointBound
		    && m_peers[i].ip == ip && m_peers[i].port == port)
			return static_cast<int>(i);
	return -1;
}

int TrackerUdpReceiver::findOrAddPeer(const uint8_t mac[6], const IPAddress &ip, uint16_t port, bool &isNew) {
    isNew = false;
    int idx = findPeerByMac(mac);
    uint8_t stableId = 0;
    if (idx >= 0) {
        if (m_peers[idx].removed) return -1;
        stableId = m_peers[idx].trackerId;
    } else {
        for (size_t i = 0; i < kMaxTrackers; i++)
            if (!m_peers[i].used) { idx = static_cast<int>(i); break; }
        if (idx < 0 || !Configuration::getInstance().getOrCreateTrackerId(mac, stableId)) return -1;
        isNew = true;
    }
    // Only invalidate previous owners after the new binding has been accepted.
    for (size_t i = 0; i < kMaxTrackers; i++) {
        auto &peer = m_peers[i];
        if (static_cast<int>(i) != idx && peer.used && peer.endpointBound && peer.ip == ip) {
            peer.endpointBound = false; peer.connected = false;
            PacketHandling::getInstance().setTrackerOnline(peer.trackerId, false);
        }
    }
    auto &peer = m_peers[idx];
    peer.used = true; peer.trackerId = stableId;
    memcpy(peer.mac, mac, 6); peer.ip = ip; peer.port = port;
    peer.endpointBound = true;
    memset(peer.accelFixed, 0, sizeof(peer.accelFixed));
    if (isNew) peer.lastHeartbeatMs = 0;
    return idx;
}

void TrackerUdpReceiver::sendHandshakeReply(const IPAddress &ip, uint16_t port) {
	uint8_t buf[1 + kHandshakeMagicLen];
	buf[0] = PKT_SRV_HANDSHAKE;
	memcpy(&buf[1], kHandshakeMagic, kHandshakeMagicLen);
	m_udp.writeTo(buf, sizeof(buf), ip, port);
}

void TrackerUdpReceiver::sendHeartbeat(Peer &p) {
	uint8_t buf[12];
	memset(buf, 0, sizeof(buf));
	buf[3] = PKT_SRV_HEARTBEAT;
	uint64_t pn = m_packetNumber++;
	for (int i = 0; i < 8; i++)
		buf[4 + i] = static_cast<uint8_t>((pn >> (8 * (7 - i))) & 0xFF);
	m_udp.writeTo(buf, sizeof(buf), p.ip, p.port);
}

void TrackerUdpReceiver::handleAccel(Peer &p, const uint8_t *data, size_t len) {
	if (len < 25) return;
	uint8_t sensorId = data[24];
	if (sensorId >= kMaxSensors) return;
	float x = readBeFloat(&data[12]);
	float y = readBeFloat(&data[16]);
	float z = readBeFloat(&data[20]);
	if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return;
	p.accelFixed[sensorId][0] = toFixed<7>(x);
	p.accelFixed[sensorId][1] = toFixed<7>(y);
	p.accelFixed[sensorId][2] = toFixed<7>(z);
}

void TrackerUdpReceiver::handleRotation(Peer &p, const uint8_t *data, size_t len) {
	if (len < 31) return;
	uint8_t sensorId = data[12];
	if (sensorId >= kMaxSensors) return;

	float qx = readBeFloat(&data[14]);
	float qy = readBeFloat(&data[18]);
	float qz = readBeFloat(&data[22]);
	float qw = readBeFloat(&data[26]);
	if (!std::isfinite(qx) || !std::isfinite(qy)
	    || !std::isfinite(qz) || !std::isfinite(qw)) return;
	int16_t qxF = toFixed<15>(qx);
	int16_t qyF = toFixed<15>(qy);
	int16_t qzF = toFixed<15>(qz);
	int16_t qwF = toFixed<15>(qw);

	uint8_t payload[15];
	payload[0] = static_cast<uint8_t>((p.trackerId << 4) | sensorId);
	memcpy(&payload[1], &qxF, 2);
	memcpy(&payload[3], &qyF, 2);
	memcpy(&payload[5], &qzF, 2);
	memcpy(&payload[7], &qwF, 2);
	memcpy(&payload[9],  &p.accelFixed[sensorId][0], 2);
	memcpy(&payload[11], &p.accelFixed[sensorId][1], 2);
	memcpy(&payload[13], &p.accelFixed[sensorId][2], 2);

	PacketHandling::getInstance().insert(payload);
}

void TrackerUdpReceiver::handleBattery(Peer &p, const uint8_t *data, size_t len) {
	if (len < 20) return;
	float voltage = readBeFloat(&data[12]);
	float pct     = readBeFloat(&data[16]);
	if (!std::isfinite(voltage) || !std::isfinite(pct)) return;

	uint8_t pctU = static_cast<uint8_t>(std::clamp(pct <= 1.0f ? pct * 100.0f : pct, 0.0f, 100.0f));
	uint16_t mv  = static_cast<uint16_t>(std::clamp(voltage, 0.0f, 6.5f) * 1000.0f);
	PacketHandling::getInstance().setBattery(p.trackerId, pctU, mv);
}

void TrackerUdpReceiver::handleTemperature(Peer &p, const uint8_t *data, size_t len) {
	if (len < 17) return;
	float tempC = readBeFloat(&data[13]);
	if (!std::isfinite(tempC)) return;
	PacketHandling::getInstance().setTemp(p.trackerId, encodeTemp(tempC), data[12]);
}

void TrackerUdpReceiver::handleSignal(Peer &p, const uint8_t *data, size_t len) {
	if (len < 14) return;
	int8_t rssi = static_cast<int8_t>(data[13]);
	PacketHandling::getInstance().setRssi(p.trackerId, rssi);
}

void TrackerUdpReceiver::handleSensorInfo(Peer &p, const uint8_t *data, size_t len) {
	if (len < 15) return;

	uint8_t imuId = data[14];
	PacketHandling::getInstance().setSensorInfo(p.trackerId, imuId, 0, data[12], data[13]);
}

void TrackerUdpReceiver::onPacket(AsyncUDPPacket &pkt) {
    std::lock_guard<std::mutex> lock(m_peerMutex);
	const uint8_t *data = pkt.data();
	size_t len = pkt.length();
	if (len < 12 || data[0] != 0 || data[1] != 0 || data[2] != 0) {
        PacketHandling::getInstance().recordRejectedUdp();
        return;
    }

	uint8_t type = data[3];

	if (type == PKT_SEND_HANDSHAKE) {
		uint8_t mac[6];
		uint32_t boardType = 0, mcuType = 0, imuType = 0;
		if (!parseHandshake(data, len, mac, boardType, mcuType, imuType)) {
			PacketHandling::getInstance().recordRejectedUdp();
			Serial.println("[Receiver] handshake parse failed");
			return;
		}
		bool isNew = false;
		int idx = findOrAddPeer(mac, pkt.remoteIP(), pkt.remotePort(), isNew);
		if (idx < 0) { Serial.println("[Receiver] tracker table full"); return; }

		sendHandshakeReply(pkt.remoteIP(), pkt.remotePort());
		sendHeartbeat(m_peers[idx]);
		m_peers[idx].lastHeartbeatMs = millis();
		m_peers[idx].lastSeenMs = millis();
		m_peers[idx].connected = true;

		if (isNew && m_onConnected)
			m_onConnected(m_peers[idx].trackerId, m_peers[idx].mac);

		PacketHandling::getInstance().setTrackerOnline(m_peers[idx].trackerId, true);
		PacketHandling::getInstance().recordReception(m_peers[idx].trackerId);

		PacketHandling::getInstance().setFirmware(
			m_peers[idx].trackerId,
			static_cast<uint8_t>(boardType),
			static_cast<uint8_t>(mcuType),
			0, 0, 0, 0
		);
		PacketHandling::getInstance().setSensorInfo(
			m_peers[idx].trackerId,
			static_cast<uint8_t>(imuType),
			0
		);
		return;
	}

	int idx = findPeerByEndpoint(pkt.remoteIP(), pkt.remotePort());
	if (idx < 0) return;
	Peer &p = m_peers[idx];
    bool valid = true;
    switch (type) {
        case PKT_SEND_ACCEL:
            valid = len >= 25 && data[24] < kMaxSensors;
            if (valid) for (unsigned offset : {12u, 16u, 20u}) valid = valid && std::isfinite(readBeFloat(data + offset));
            break;
        case PKT_SEND_ROTATION:
            valid = len >= 31 && data[12] < kMaxSensors;
            if (valid) for (unsigned offset : {14u, 18u, 22u, 26u}) valid = valid && std::isfinite(readBeFloat(data + offset));
            break;
        case PKT_SEND_BATTERY:
            valid = len >= 20 && std::isfinite(readBeFloat(data + 12)) && std::isfinite(readBeFloat(data + 16)); break;
        case PKT_SEND_TEMP:
            valid = len >= 17 && data[12] < kMaxSensors && std::isfinite(readBeFloat(data + 13)); break;
        case PKT_SEND_SIGNAL: valid = len >= 14; break;
        case PKT_SEND_SENSORINFO: valid = len >= 15 && data[12] < kMaxSensors; break;
        default: break;
    }
    if (!valid) { PacketHandling::getInstance().recordRejectedUdp(p.trackerId); return; }
    PacketHandling::getInstance().recordReception(p.trackerId);

	p.lastSeenMs = millis();
	if (!p.connected) {
		p.connected = true;
		PacketHandling::getInstance().setTrackerOnline(p.trackerId, true);
	}

	switch (type) {
		case PKT_SEND_ACCEL:      handleAccel(p, data, len);       break;
		case PKT_SEND_ROTATION:   handleRotation(p, data, len);    break;
		case PKT_SEND_BATTERY:    handleBattery(p, data, len);     break;
		case PKT_SEND_TEMP:       handleTemperature(p, data, len); break;
		case PKT_SEND_SIGNAL:     handleSignal(p, data, len);      break;
		case PKT_SEND_SENSORINFO: handleSensorInfo(p, data, len);  break;
		default: break;
	}
}

TrackerUdpReceiver::ForgetResult TrackerUdpReceiver::forgetTracker(uint8_t trackerId) {
    std::lock_guard<std::mutex> lock(m_peerMutex);
    int idx = -1;
    for (size_t i = 0; i < kMaxTrackers; i++) {
        auto &peer = m_peers[i];
        if (!peer.used || peer.trackerId != trackerId) continue;
        idx = static_cast<int>(i);
        if (peer.connected && uint32_t(millis() - peer.lastSeenMs) < kTrackerTimeoutMs)
            return ForgetResult::Active;
        break;
    }
    if (!Configuration::getInstance().removeTracker(trackerId)) return ForgetResult::Failed;
    if (idx >= 0) {
        m_peers[idx].connected = false;
        m_peers[idx].endpointBound = false;
        m_peers[idx].removed = true;
    }
    PacketHandling::getInstance().forgetTracker(trackerId);
    return ForgetResult::Removed;
}

void TrackerUdpReceiver::update() {
    std::lock_guard<std::mutex> lock(m_peerMutex);
	uint32_t now = millis();
	for (size_t i = 0; i < kMaxTrackers; i++) {
		Peer &p = m_peers[i];
		if (!p.used || !p.endpointBound) continue;

		if (kEnableHeartbeat
		    && static_cast<int32_t>(now - p.lastHeartbeatMs) >= static_cast<int32_t>(kHeartbeatIntervalMs)) {
			p.lastHeartbeatMs = now;
			sendHeartbeat(p);
		}

		if (p.connected
		    && static_cast<int32_t>(now - p.lastSeenMs) >= static_cast<int32_t>(kTrackerTimeoutMs)) {
			p.connected = false;
			Serial.printf("[Receiver] tracker id=%u timed out -> disconnected\n", p.trackerId);
			PacketHandling::getInstance().setTrackerOnline(p.trackerId, false);
		}
	}
}
