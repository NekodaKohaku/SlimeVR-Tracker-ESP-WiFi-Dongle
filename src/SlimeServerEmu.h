/*
	Official-compatible UDP proxy for stock SlimeVR ESP trackers.
*/

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

	// 追蹤器送出
	static constexpr uint8_t PKT_SEND_HANDSHAKE  = 3;
	static constexpr uint8_t PKT_SEND_ACCEL      = 4;
	static constexpr uint8_t PKT_SEND_BATTERY    = 12;
	static constexpr uint8_t PKT_SEND_SENSORINFO = 15;
	static constexpr uint8_t PKT_SEND_ROTATION   = 17;
	static constexpr uint8_t PKT_SEND_SIGNAL     = 19;
	static constexpr uint8_t PKT_SEND_TEMP       = 20;
	// 伺服器送出
	static constexpr uint8_t PKT_SRV_HEARTBEAT   = 1;
	static constexpr uint8_t PKT_SRV_HANDSHAKE   = 3;

	// sensorId 是 4-bit(0..15):主感測器=0,副追蹤器(擴充)=1..15。
	static constexpr size_t kMaxSensorsPerTracker = 16;

	struct Peer {
		bool      used = false;
		uint8_t   trackerId = 0;
		uint8_t   mac[6] = {0};
		IPAddress ip;
		uint16_t  port = 0;
		uint32_t  lastHeartbeatMs = 0;
		// 最新 accel,組 rotation 封包時用。必須「每個 sensorId 各存一份」:
		// 追蹤器會對主 IMU 與副 IMU 各送一個 accel 封包(sensorId 不同),
		// 共用一份的話後到的會蓋掉先到的 → 兩顆感測器都拿到對方的加速度。
		int16_t   accelFixed[kMaxSensorsPerTracker][3] = {};
		uint32_t  lastSeenMs = 0;              // 最後收到該顆任何封包的時間
		bool      connected = false;           // 目前是否視為連線中
	};

	Peer m_peers[kMaxTrackers];
	AsyncUDP m_udp;
	uint64_t m_packetNumber = 0;
	uint8_t  m_maxConn = 12;

	// 自動選台(apChannel=0)的結果,0 = 還沒選過。
	// begin() 會被開機失敗的重試路徑重複呼叫,但阻塞式通道掃描只能做一次,
	// 理由見 begin() 內的說明。
	uint8_t  m_pickedChannel = 0;

	// 被配對表拒絕過的 MAC。被拒代表「表已滿且這顆不在表內」,
	// 而配對表只有五連按(會接著重開機)才會變動 → 同一顆 MAC 在本次開機內
	// 不可能突然變成可接受。快取起來,避免它每秒重試一次就掃一次 flash:
	// 那個掃描發生在 lwIP task 上,LittleFS 讀取會阻塞整個網路堆疊,
	// 連帶讓其他正常追蹤器的封包延遲甚至逾時。
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

	// 各封包解析(拿到就更新 PacketHandling 對應欄位)
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
		// 夾住的動作必須在「浮點域」做,不能先轉 int32 再 clamp。
		// float→整數轉換在來源值超出目標範圍或為 NaN/Inf 時是 undefined behavior;
		// Xtensa 的轉換指令在這種情況會給出未定義的位元組合(不保證飽和),
		// 於是 clamp 收到的已經是垃圾值,夾了也沒用 —— 姿態會瞬間跳到隨機方向。
		// 這一層是「絕不產生 UB」的底線,不依賴任何關於對方會送什麼的假設:
		// 進來的是 UDP 上的 32-bit 位元組合,融合演算法發散、封包損毀、
		// 或某個韌體分支的行為改變,都可能讓它是 NaN/Inf。
		// (語意層的處理在各個 handler:handleRotation 對非有限值直接丟整包。)
		if (!std::isfinite(number)) return 0;
		float scaled = number * static_cast<float>(1u << Q);
		scaled = std::clamp(scaled, -32768.0f, 32767.0f);
		return static_cast<int16_t>(scaled);
	}
	// 溫度編碼(對齊你客製 sendInfo 的編碼:(t-25)*2 + 128.5,clamp 1..255)
	static uint8_t encodeTemp(float tempC) {
		// NaN 的比較全部為 false → 兩個 if 都不成立 → 直接把 NaN 轉成 uint8_t(UB)。
		// 同樣是純防禦:官方韌體的溫度欄位未讀取時是 0
		// (tracker-esp/src/sensors/bno080sensor.h:109 `float lastReadTemperature = 0`),
		// 但這個值一樣是從 UDP 上照收的 32-bit 位元組合,不能假設它一定有限。
		if (!std::isfinite(tempC)) return 1;   // 1 = 編碼範圍下限(server 視為極低溫但不會亂跳)
		float e = (tempC - 25.0f) * 2.0f + 128.5f;
		if (e < 1.0f) e = 1.0f;
		if (e > 255.0f) e = 255.0f;
		return static_cast<uint8_t>(e);
	}
};
