

#include "SlimeServerEmu.h"
#include "packetHandling.h"
#include "configuration.h"
#include "esp_wifi.h"

SlimeServerEmu SlimeServerEmu::instance;

static const char kHandshakeMagic[] = "Hey OVR =D 5";
static constexpr size_t kHandshakeMagicLen = 12;

SlimeServerEmu &SlimeServerEmu::getInstance() { return instance; }

ErrorCodes SlimeServerEmu::begin(
	const char *ssid, const char *password,
	uint8_t channel, uint8_t maxConn, bool hidden
) {
	m_maxConn = maxConn;

	uint8_t useChannel = channel;
	if (channel == 0) {
		useChannel = pickBestChannel();
	}

	WiFi.mode(WIFI_AP);
	WiFi.softAP(ssid, password, useChannel, hidden ? 1 : 0, maxConn);

	wifi_config_t cfg;
	esp_wifi_get_config(WIFI_IF_AP, &cfg);
	cfg.ap.beacon_interval = kBeaconIntervalMs;
	cfg.ap.dtim_period     = kDtimPeriod;
	esp_wifi_set_config(WIFI_IF_AP, &cfg);
	esp_wifi_set_ps(WIFI_PS_NONE);

	if (m_udp.listen(kPort)) {
		m_udp.onPacket([this](AsyncUDPPacket pkt) { onPacket(pkt); });
	}
	Serial.printf("[Emu] SlimeVR server emulator on ch %u, listening on :%u\n", useChannel, kPort);
	return ErrorCodes::NO_ERROR;
}

uint8_t SlimeServerEmu::pickBestChannel() {
	WiFi.mode(WIFI_AP_STA);
	int n = WiFi.scanNetworks(false, false);
	if (n <= 0) {
		WiFi.scanDelete();
		Serial.println("[Emu] channel scan: no APs found, default ch 1");
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

	Serial.printf("[Emu] channel scan: ch1=%ld ch6=%ld ch11=%ld -> pick ch %u\n",
	              score[0], score[1], score[2], cands[best]);
	return cands[best];
}

uint8_t SlimeServerEmu::connectedCount() const {
	uint8_t n = 0;
	for (size_t i = 0; i < kMaxTrackers; i++) if (m_peers[i].used) n++;
	return n;
}

bool SlimeServerEmu::parseHandshake(const uint8_t *data, size_t len, uint8_t outMac[6],
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

int SlimeServerEmu::findPeerByMac(const uint8_t mac[6]) {
	for (size_t i = 0; i < kMaxTrackers; i++)
		if (m_peers[i].used && memcmp(m_peers[i].mac, mac, 6) == 0)
			return static_cast<int>(i);
	return -1;
}

int SlimeServerEmu::findPeerByIp(const IPAddress &ip) {
	for (size_t i = 0; i < kMaxTrackers; i++)
		if (m_peers[i].used && m_peers[i].ip == ip)
			return static_cast<int>(i);
	return -1;
}

int SlimeServerEmu::findOrAddPeer(const uint8_t mac[6], const IPAddress &ip, uint16_t port, bool &isNew) {
	isNew = false;
	int idx = findPeerByMac(mac);
	if (idx >= 0) {
		m_peers[idx].ip = ip;
		m_peers[idx].port = port;
		return idx;
	}

	uint8_t stableId = 0;
	if (!Configuration::getInstance().getOrCreateTrackerId(mac, stableId)) {
		Serial.println("[Emu] pairing table full, tracker rejected (5-press button to reset)");
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
			Serial.printf("[Emu] new tracker id=%u mac=%02x:%02x:%02x:%02x:%02x:%02x ip=%s\n",
				stableId, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],
				ip.toString().c_str());
			return static_cast<int>(i);
		}
	}
	return -1;
}

void SlimeServerEmu::sendHandshakeReply(const IPAddress &ip, uint16_t port) {
	uint8_t buf[1 + kHandshakeMagicLen];
	buf[0] = PKT_SRV_HANDSHAKE;
	memcpy(&buf[1], kHandshakeMagic, kHandshakeMagicLen);
	m_udp.writeTo(buf, sizeof(buf), ip, port);
}

void SlimeServerEmu::sendHeartbeat(Peer &p) {
	uint8_t buf[12];
	memset(buf, 0, sizeof(buf));
	buf[3] = PKT_SRV_HEARTBEAT;
	uint64_t pn = m_packetNumber++;
	for (int i = 0; i < 8; i++)
		buf[4 + i] = static_cast<uint8_t>((pn >> (8 * (7 - i))) & 0xFF);
	m_udp.writeTo(buf, sizeof(buf), p.ip, p.port);
}

void SlimeServerEmu::handleAccel(Peer &p, const uint8_t *data, size_t len) {
	if (len < 25) return;
	p.accelFixed[0] = toFixed<7>(readBeFloat(&data[12]));
	p.accelFixed[1] = toFixed<7>(readBeFloat(&data[16]));
	p.accelFixed[2] = toFixed<7>(readBeFloat(&data[20]));
}

void SlimeServerEmu::handleRotation(Peer &p, const uint8_t *data, size_t len) {
	if (len < 31) return;
	uint8_t sensorId = data[12] & 0x0F;

	int16_t qxF = toFixed<15>(readBeFloat(&data[14]));
	int16_t qyF = toFixed<15>(readBeFloat(&data[18]));
	int16_t qzF = toFixed<15>(readBeFloat(&data[22]));
	int16_t qwF = toFixed<15>(readBeFloat(&data[26]));

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

void SlimeServerEmu::handleBattery(Peer &p, const uint8_t *data, size_t len) {
	if (len < 20) return;
	float voltage = readBeFloat(&data[12]);
	float pct     = readBeFloat(&data[16]);

	uint8_t pctU = static_cast<uint8_t>(std::clamp(pct <= 1.0f ? pct * 100.0f : pct, 0.0f, 100.0f));
	uint16_t mv  = static_cast<uint16_t>(std::clamp(voltage, 0.0f, 6.5f) * 1000.0f);
	PacketHandling::getInstance().setBattery(p.trackerId, pctU, mv);
}

void SlimeServerEmu::handleTemperature(Peer &p, const uint8_t *data, size_t len) {
	if (len < 17) return;
	float tempC = readBeFloat(&data[13]);
	PacketHandling::getInstance().setTemp(p.trackerId, encodeTemp(tempC));
}

void SlimeServerEmu::handleSignal(Peer &p, const uint8_t *data, size_t len) {
	if (len < 14) return;
	int8_t rssi = static_cast<int8_t>(data[13]);
	PacketHandling::getInstance().setRssi(p.trackerId, rssi);
}

void SlimeServerEmu::handleSensorInfo(Peer &p, const uint8_t *data, size_t len) {
	if (len < 15) return;

	uint8_t imuId = data[14];
	PacketHandling::getInstance().setSensorInfo(p.trackerId, imuId, 0);
}

void SlimeServerEmu::onPacket(AsyncUDPPacket &pkt) {
	const uint8_t *data = pkt.data();
	size_t len = pkt.length();
	if (len < 4) return;

	uint8_t type = data[3];

	if (type == PKT_SEND_HANDSHAKE) {
		uint8_t mac[6];
		uint32_t boardType = 0, mcuType = 0, imuType = 0;
		if (!parseHandshake(data, len, mac, boardType, mcuType, imuType)) {
			Serial.println("[Emu] handshake parse failed");
			return;
		}
		bool isNew = false;
		int idx = findOrAddPeer(mac, pkt.remoteIP(), pkt.remotePort(), isNew);
		if (idx < 0) { Serial.println("[Emu] tracker table full"); return; }

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

void SlimeServerEmu::update() {
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
			Serial.printf("[Emu] tracker id=%u timed out -> disconnected\n", p.trackerId);
			PacketHandling::getInstance().setTrackerOnline(p.trackerId, false);
		}
	}
}
