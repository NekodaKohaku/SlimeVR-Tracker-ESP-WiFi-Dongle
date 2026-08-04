

#include "TrackerUdpReceiver.h"
#include "packetHandling.h"
#include "configuration.h"
#include "esp_wifi.h"

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
	int n = WiFi.scanNetworks(false, false);
	if (n <= 0) {
		WiFi.scanDelete();
		Serial.println("[Receiver] channel scan: no APs found, default ch 1");
		return 1;
	}

	const uint8_t cands[3] = {1, 6, 11};
	long score[3] = {0, 0, 0};

	for (int i = 0; i < n; i++) {
		int ch = WiFi.channel(i);
		int rssi = WiFi.RSSI(i);
		long w = rssi + 100;
		if (w < 1) w = 1;
		for (int k = 0; k < 3; k++) {
			int dist = abs(ch - static_cast<int>(cands[k]));
			if (dist == 0)      score[k] += w * 4;
			else if (dist <= 2) score[k] += w * 2;
			else if (dist <= 4) score[k] += w * 1;
		}
	}
	WiFi.scanDelete();

	int best = 0;
	for (int k = 1; k < 3; k++) if (score[k] < score[best]) best = k;

	Serial.printf("[Receiver] channel scan: ch1=%ld ch6=%ld ch11=%ld -> pick ch %u\n",
	              score[0], score[1], score[2], cands[best]);
	return cands[best];
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

int TrackerUdpReceiver::findPeerByIp(const IPAddress &ip) {
	for (size_t i = 0; i < kMaxTrackers; i++)
		if (m_peers[i].used && m_peers[i].ip == ip)
			return static_cast<int>(i);
	return -1;
}

int TrackerUdpReceiver::findOrAddPeer(const uint8_t mac[6], const IPAddress &ip, uint16_t port, bool &isNew) {
	isNew = false;
	int idx = findPeerByMac(mac);
	if (idx >= 0) {
		m_peers[idx].ip = ip;
		m_peers[idx].port = port;
		return idx;
	}

	uint8_t stableId = 0;
	if (!Configuration::getInstance().getOrCreateTrackerId(mac, stableId)) {
		Serial.println("[Receiver] tracker table full (press BOOT 5 times to reset)");
		return -1;
	}
	for (size_t i = 0; i < kMaxTrackers; i++) {
		if (!m_peers[i].used) {
			m_peers[i].used = true;
			m_peers[i].trackerId = stableId;
			memcpy(m_peers[i].mac, mac, 6);
			m_peers[i].ip = ip;
			m_peers[i].port = port;
			m_peers[i].lastHeartbeatMs = 0;
			isNew = true;
			Serial.printf("[Receiver] new tracker id=%u mac=%02x:%02x:%02x:%02x:%02x:%02x ip=%s\n",
				stableId, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],
				ip.toString().c_str());
			return static_cast<int>(i);
		}
	}
	return -1;
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
	float x = readBeFloat(&data[12]);
	float y = readBeFloat(&data[16]);
	float z = readBeFloat(&data[20]);
	if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return;
	p.accelFixed[0] = toFixed<7>(x);
	p.accelFixed[1] = toFixed<7>(y);
	p.accelFixed[2] = toFixed<7>(z);
}

void TrackerUdpReceiver::handleRotation(Peer &p, const uint8_t *data, size_t len) {
	if (len < 31) return;
	uint8_t sensorId = data[12] & 0x0F;

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
	memcpy(&payload[9],  &p.accelFixed[0], 2);
	memcpy(&payload[11], &p.accelFixed[1], 2);
	memcpy(&payload[13], &p.accelFixed[2], 2);

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
	PacketHandling::getInstance().setTemp(p.trackerId, encodeTemp(tempC));
}

void TrackerUdpReceiver::handleSignal(Peer &p, const uint8_t *data, size_t len) {
	if (len < 14) return;
	int8_t rssi = static_cast<int8_t>(data[13]);
	PacketHandling::getInstance().setRssi(p.trackerId, rssi);
}

void TrackerUdpReceiver::handleSensorInfo(Peer &p, const uint8_t *data, size_t len) {
	if (len < 15) return;

	uint8_t imuId = data[14];
	PacketHandling::getInstance().setSensorInfo(p.trackerId, imuId, 0);
}

void TrackerUdpReceiver::onPacket(AsyncUDPPacket &pkt) {
	const uint8_t *data = pkt.data();
	size_t len = pkt.length();
	if (len < 4) return;

	uint8_t type = data[3];

	if (type == PKT_SEND_HANDSHAKE) {
		uint8_t mac[6];
		uint32_t boardType = 0, mcuType = 0, imuType = 0;
		if (!parseHandshake(data, len, mac, boardType, mcuType, imuType)) {
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

	int idx = findPeerByIp(pkt.remoteIP());
	if (idx < 0) return;
	Peer &p = m_peers[idx];

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

void TrackerUdpReceiver::update() {
	uint32_t now = millis();
	for (size_t i = 0; i < kMaxTrackers; i++) {
		Peer &p = m_peers[i];
		if (!p.used) continue;

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
