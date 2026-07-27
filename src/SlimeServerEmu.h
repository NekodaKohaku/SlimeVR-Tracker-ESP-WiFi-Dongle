#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <functional>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>

#include "error_codes.h"
#include "WifiDongleConfig.h"

class SlimeServerEmu {
public:
	static SlimeServerEmu &getInstance();

	ErrorCodes begin(
		const char *ssid,
		const char *password,
		uint8_t channel,
		uint8_t maxConn,
		bool hidden
	);

	void update();

	void onTrackerConnected(std::function<void(uint8_t trackerId, const uint8_t *mac)> cb) {
		m_onConnected = std::move(cb);
	}

	uint8_t connectedCount() const;

private:
	SlimeServerEmu() = default;
	static SlimeServerEmu instance;

	static constexpr uint16_t kPort = WifiDongleConfig::udpPort;
	static constexpr size_t   kMaxTrackers = WifiDongleConfig::maxTrackers;

	static constexpr bool     kEnableHeartbeat    = WifiDongleConfig::officialHeartbeatEnabled;
	static constexpr uint32_t kHeartbeatIntervalMs = WifiDongleConfig::officialHeartbeatIntervalMs;

	static constexpr uint16_t kBeaconIntervalMs = WifiDongleConfig::beaconIntervalMs;
	static constexpr uint8_t  kDtimPeriod       = WifiDongleConfig::dtimPeriod;
	static constexpr uint32_t kTrackerTimeoutMs = WifiDongleConfig::officialTrackerTimeoutMs;

	static constexpr uint8_t PKT_SEND_HANDSHAKE  = 3;
	static constexpr uint8_t PKT_SEND_ACCEL      = 4;
	static constexpr uint8_t PKT_SEND_BATTERY    = 12;
	static constexpr uint8_t PKT_SEND_SENSORINFO = 15;
	static constexpr uint8_t PKT_SEND_ROTATION   = 17;
	static constexpr uint8_t PKT_SEND_SIGNAL     = 19;
	static constexpr uint8_t PKT_SEND_TEMP       = 20;

	static constexpr uint8_t PKT_SRV_HEARTBEAT   = 1;
	static constexpr uint8_t PKT_SRV_HANDSHAKE   = 3;

	static constexpr size_t kMaxSensorsPerTracker = 16;

	struct Peer {
		bool      used = false;
		uint8_t   trackerId = 0;
		uint8_t   mac[6] = {0};
		IPAddress ip;
		uint16_t  port = 0;
		uint32_t  lastHeartbeatMs = 0;

		int16_t   accelFixed[kMaxSensorsPerTracker][3] = {};
		uint32_t  lastSeenMs = 0;
		bool      connected = false;
	};

	Peer m_peers[kMaxTrackers];
	AsyncUDP m_udp;
	uint64_t m_packetNumber = 0;
	uint8_t  m_maxConn = 12;

	uint8_t  m_pickedChannel = 0;

	uint8_t  m_rejectedMac[6] = {0};
	bool     m_hasRejectedMac = false;

	std::function<void(uint8_t, const uint8_t *)> m_onConnected;

	void onPacket(AsyncUDPPacket &pkt);
	uint8_t pickBestChannel();
	bool parseHandshake(const uint8_t *data, size_t len, uint8_t outMac[6],
	                    uint32_t &boardType, uint32_t &mcuType, uint32_t &imuType);
	int  findPeerByMac(const uint8_t mac[6]);
	int  findPeerByIp(const IPAddress &ip);
	void releaseDuplicateIp(const IPAddress &ip, int keepIdx);
	int  findOrAddPeer(const uint8_t mac[6], const IPAddress &ip, uint16_t port, bool &isNew);
	void sendHandshakeReply(const IPAddress &ip, uint16_t port);
	void sendHeartbeat(Peer &p);

	void handleRotation(Peer &p, const uint8_t *data, size_t len);
	void handleAccel(Peer &p, const uint8_t *data, size_t len);
	void handleBattery(Peer &p, const uint8_t *data, size_t len);
	void handleTemperature(Peer &p, const uint8_t *data, size_t len);
	void handleSignal(Peer &p, const uint8_t *data, size_t len);
	void handleSensorInfo(Peer &p, const uint8_t *data, size_t len);

	static float readBeFloat(const uint8_t *p) {
		uint8_t le[4] = { p[3], p[2], p[1], p[0] };
		float f;
		memcpy(&f, le, 4);
		return f;
	}
	static uint32_t readBeU32(const uint8_t *p) {
		return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
		     | (static_cast<uint32_t>(p[2]) << 8)  |  static_cast<uint32_t>(p[3]);
	}
	template <unsigned Q>
	static int16_t toFixed(float number) {

		if (!std::isfinite(number)) return 0;
		float scaled = number * static_cast<float>(1u << Q);
		scaled = std::clamp(scaled, -32768.0f, 32767.0f);
		return static_cast<int16_t>(scaled);
	}

	static uint8_t encodeTemp(float tempC) {

		if (!std::isfinite(tempC)) return 1;
		float e = (tempC - 25.0f) * 2.0f + 128.5f;
		if (e < 1.0f) e = 1.0f;
		if (e > 255.0f) e = 255.0f;
		return static_cast<uint8_t>(e);
	}
};
