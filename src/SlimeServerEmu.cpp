/*
	Official-compatible UDP proxy for stock SlimeVR ESP trackers.
*/

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
		// 掃描只做一次,之後的重試沿用同一個通道。
		// begin() 不再只在開機時被呼叫:開機失敗後 loop() 每 10 秒會重試一次,
		// 而 WiFi.scanNetworks(false, false) 是「阻塞」掃描(全通道約 2~4 秒),
		// 跑在 loop task 上等於這幾秒內 button.update() 完全不輪詢 ——
		// 五連按重設配對表靠連續輪詢做去彈跳與計數,漏掉中間幾按只會數成 2、3 下
		// 而不觸發。也就是說「重試」本身會打壞它要保住的救援路徑。
		// 第一次掃出來的結果對重試而言也夠用:環境在這幾十秒內不會變。
		if (m_pickedChannel == 0) {
			m_pickedChannel = pickBestChannel();
		}
		useChannel = m_pickedChannel;
	}

	// 重試前先關掉上一輪可能已經開起來的 socket。
	// 走到重試代表上一次 begin() 是中途 return 的:AP_START_FAILED 時 listen 還沒跑,
	// 但 UDP_LISTEN_FAILED 時 AsyncUDP 內部可能已經配置了一半的 PCB。
	// 對沒開過的 AsyncUDP 呼叫 close() 是安全的空操作,所以無條件先關。
	m_udp.close();

	WiFi.mode(WIFI_AP);
	// softAP() 的回傳值一定要看。失敗的常見原因:密碼長度不合法(1..7 字元)、
	// 通道超出範圍、NVS 分割區異常。忽略它的話,失敗時 dongle 會「開機正常但
	// 追蹤器永遠連不上」,而 LED 什麼都不顯示 —— 從外部完全無法診斷。
	if (!WiFi.softAP(ssid, password, useChannel, hidden ? 1 : 0, maxConn)) {
		Serial.println("[Emu] softAP start FAILED");
		return ErrorCodes::AP_START_FAILED;
	}

	// beacon interval / DTIM 是「讀出現有設定 → 改兩個欄位 → 寫回」。
	// get 失敗時 cfg 的內容沒有定義,直接寫回去等於把整份 AP 設定(SSID、密碼、
	// 認證模式、最大連線數)換成堆疊上的垃圾 —— 剛剛才成功起來的 AP 會當場變成
	// 一個誰也連不上、名字是亂碼的 AP,而回傳值沒人看,序列埠上一片安靜。
	// 這兩個欄位只是延遲/省電的微調,拿不到就跳過,AP 照樣是好的。
	wifi_config_t cfg = {};
	esp_err_t cfgErr = esp_wifi_get_config(WIFI_IF_AP, &cfg);
	if (cfgErr == ESP_OK) {
		cfg.ap.beacon_interval = kBeaconIntervalMs;
		cfg.ap.dtim_period     = kDtimPeriod;
		esp_err_t setErr = esp_wifi_set_config(WIFI_IF_AP, &cfg);
		if (setErr != ESP_OK)
			Serial.printf("[Emu] beacon/DTIM tuning skipped (set_config: %d)\n",
			              static_cast<int>(setErr));
	} else {
		Serial.printf("[Emu] beacon/DTIM tuning skipped (get_config: %d)\n",
		              static_cast<int>(cfgErr));
	}
	esp_wifi_set_ps(WIFI_PS_NONE);

	if (!m_udp.listen(kPort)) {
		Serial.printf("[Emu] UDP listen on :%u FAILED\n", kPort);
		return ErrorCodes::UDP_LISTEN_FAILED;
	}
	m_udp.onPacket([this](AsyncUDPPacket pkt) { onPacket(pkt); });

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
	// 算「目前連線中」而不是「占用槽位」:離線的 peer 槽會一直留著等重連
	//(這棵樹裡沒有任何地方回收 peer 槽,used 一旦為 true 就不會再變回 false),
	// 用 used 統計的話數字只增不減,和使用者看到的狀態對不上。
	for (size_t i = 0; i < kMaxTrackers; i++) if (m_peers[i].used && m_peers[i].connected) n++;
	return n;
}

// discovery handshake 佈局(tracker -> server):
//   [0..2]0 [3]3 [4..11]packetNum
//   body@12: board(BE u32) imu(4) mcu(BE u32) imuInfo*3(12) protocol(4)
//   @40: fwVersion shortstring(1+N)   @41+N: MAC(6)
bool SlimeServerEmu::parseHandshake(const uint8_t *data, size_t len, uint8_t outMac[6],
                                    uint32_t &boardType, uint32_t &mcuType, uint32_t &imuType) {
	constexpr size_t kBodyOffset = 12;
	constexpr size_t kFwLenOffset = 40;
	if (len <= kFwLenOffset) return false;

	boardType = readBeU32(&data[kBodyOffset]);       // @12
	imuType   = readBeU32(&data[kBodyOffset + 4]);    // @16
	mcuType   = readBeU32(&data[kBodyOffset + 8]);    // @20

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

// 一個 IP 同時只能屬於一個 peer。SoftAP 的 DHCP 可能把離線追蹤器用過的 IP
// 重新發給另一顆 MAC;舊那筆若還留著同一個 IP,之後 findPeerByIp() 會先命中舊的,
// 新追蹤器的資料就被算到舊追蹤器頭上(部位錯亂,且新的那顆看起來完全沒資料)。
// 把其他 peer 上重複的 IP 作廢;該 peer 之後會自然逾時下線。
//
// 呼叫時機的限制:「只能在確定 handshake 會成功之後」才呼叫,絕不能在函式開頭。
// 擺在開頭的話,一顆連配對表都進不去、注定被回絕的追蹤器,照樣會把一顆正常
// 運作中的追蹤器的 IP 清成 0.0.0.0 —— 那顆好端端的追蹤器從此收不到心跳、被判
// 逾時,而真正的元凶什麼也沒得到。回絕是每秒重試一次的,受害者會被持續打。
void SlimeServerEmu::releaseDuplicateIp(const IPAddress &ip, int keepIdx) {
	for (size_t i = 0; i < kMaxTrackers; i++) {
		if (!m_peers[i].used) continue;
		if (static_cast<int>(i) == keepIdx) continue;
		if (m_peers[i].ip == ip) {
			Serial.printf("[Emu] ip %s reassigned, releasing stale peer id=%u\n",
				ip.toString().c_str(), m_peers[i].trackerId);
			m_peers[i].ip = IPAddress();   // 0.0.0.0:不會再被任何封包命中
		}
	}
}

int SlimeServerEmu::findOrAddPeer(const uint8_t mac[6], const IPAddress &ip, uint16_t port, bool &isNew) {
	isNew = false;
	int idx = findPeerByMac(mac);

	if (idx >= 0) {
		releaseDuplicateIp(ip, idx);
		m_peers[idx].ip = ip;
		m_peers[idx].port = port;
		return idx;
	}

	// 這顆 MAC 之前已經被配對表拒絕過 → 直接回絕,不要再掃一次 flash。
	// (掃描跑在 lwIP task 上,會阻塞整個網路堆疊)
	if (m_hasRejectedMac && memcmp(m_rejectedMac, mac, 6) == 0) return -1;

	// 先確認有 peer 槽,再去碰配對表 —— 在確定會成功之前不改動任何既有狀態。
	//
	// 這裡刻意不做「回收閒置槽」:每一個 used 的 peer 槽都對應配對表裡的一筆,
	// 所以「已用槽數 <= 配對表筆數 <= maxTrackers」永遠成立。槽滿的時候配對表
	// 一定也滿了,就算回收出一個槽,接下來的 getOrCreateTrackerId() 照樣配不到 id,
	// 淨效果只是把一顆合法追蹤器的狀態白清掉。要騰出空間只能五連按重設配對表。
	int slot = -1;
	for (size_t i = 0; i < kMaxTrackers; i++) {
		if (!m_peers[i].used) { slot = static_cast<int>(i); break; }
	}
	if (slot < 0) {
		Serial.println("[Emu] peer table full, tracker rejected (5-press button to reset)");
		memcpy(m_rejectedMac, mac, 6);
		m_hasRejectedMac = true;
		return -1;
	}

	// trackerId 改用持久化配對表(LittleFS):同一顆 MAC 永遠拿到同一個 id,dongle 重開機不變。
	// 若照連線順序分配,重開後 id 洗牌:主感測器的 hidId 都是 server 見過的 → 資料流進
	// 「別顆的舊欄位」(部位互換,難察覺);副感測器 hidId=(sensorId<<4)|trackerId 內含
	// trackerId,洗牌後變成 server 沒見過的值 → 被當成新裝置重複註冊,舊的餓死變已逾時。
	// 配對表滿了可用按鈕五連按(resetTrackers)清除。
	uint8_t stableId = 0;
	TrackerIdStatus st = Configuration::getInstance().getOrCreateTrackerId(mac, stableId);
	if (st != TrackerIdStatus::Ok) {
		// 只有 Permanent(表滿/表壞)才快取這顆 MAC。Transient 是 flash 寫入失敗,
		// 下一次 handshake 可能就成功了 —— 快取它會讓一次偶發的寫入錯誤把這顆追蹤器
		// 鎖在門外直到重開機。代價只是它每秒重試時會多掃一次 flash。
		if (st == TrackerIdStatus::Permanent) {
			Serial.println("[Emu] pairing table full, tracker rejected (5-press button to reset)");
			memcpy(m_rejectedMac, mac, 6);
			m_hasRejectedMac = true;
		} else {
			Serial.println("[Emu] pairing storage error, will retry on next handshake");
		}
		return -1;
	}

	// 到這裡才確定會成功,現在才可以動既有 peer 的狀態(見 releaseDuplicateIp 說明)。
	releaseDuplicateIp(ip, -1);

	// 先填內容、最後才設 used=true:update()(loop task)隨時在掃這個陣列,
	// 先設 used 的話它可能讀到還沒填好的 ip/port 而對 0.0.0.0 發心跳。
	m_peers[slot].trackerId = stableId;
	memcpy(m_peers[slot].mac, mac, 6);
	m_peers[slot].ip = ip;
	m_peers[slot].port = port;
	m_peers[slot].lastHeartbeatMs = 0;
	m_peers[slot].lastSeenMs = millis();
	m_peers[slot].connected = false;   // 由 update() 統一依 lastSeenMs 判定
	memset(m_peers[slot].accelFixed, 0, sizeof(m_peers[slot].accelFixed));
	m_peers[slot].used = true;         // 最後設

	isNew = true;
	Serial.printf("[Emu] new tracker id=%u mac=%02x:%02x:%02x:%02x:%02x:%02x ip=%s\n",
		stableId, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],
		ip.toString().c_str());
	return slot;
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

// ---- Accel(4):body@12 = x,y,z (f32 BE), sensorId(1). len=25 ----
// 官方韌體 packets.h 有 #pragma pack(push,1),AccelPacket 不含填充,
// 所以 x@12 y@16 z@20 sensorId@24(總長 12+13=25,與上面的長度檢查一致)。
// sensorId 一定要讀:一台板子帶副 IMU 時,主/副會各送一個 accel 封包,
// 若都寫進同一份緩衝,後到的會蓋掉先到的 → 兩顆感測器的加速度互相污染。
void SlimeServerEmu::handleAccel(Peer &p, const uint8_t *data, size_t len) {
	if (len < 25) return;
	uint8_t sensorId = data[24] & 0x0F;   // & 0x0F 同時保證索引不會越界(陣列大小 16)
	p.accelFixed[sensorId][0] = toFixed<7>(readBeFloat(&data[12]));
	p.accelFixed[sensorId][1] = toFixed<7>(readBeFloat(&data[16]));
	p.accelFixed[sensorId][2] = toFixed<7>(readBeFloat(&data[20]));
}

// ---- RotationData(17):[12]sensorId [13]dataType [14..29]quat xyzw(f32 BE) [30]acc ----
void SlimeServerEmu::handleRotation(Peer &p, const uint8_t *data, size_t len) {
	if (len < 31) return;
	uint8_t sensorId = data[12] & 0x0F;

	float qx = readBeFloat(&data[14]);
	float qy = readBeFloat(&data[18]);
	float qz = readBeFloat(&data[22]);
	float qw = readBeFloat(&data[26]);
	// 四元數只要有一個分量不是有限值,整包就沒有意義,直接丟掉。
	// toFixed 對非有限值會回傳 0(見 SlimeServerEmu.h),所以硬送下去不會是垃圾位元,
	// 但會變成 (0,0,0,0) 這種「合法編碼、無意義旋轉」的四元數 ——
	// server 收到後照樣會拿去正規化並套用到骨架上,使用者看到的是肢體扭到
	// 一個奇怪角度,而不是「這顆感測器沒資料」。
	// 在這裡整包丟掉的話 server 沿用上一筆,畫面靜止,問題一眼就看得出來。
	// (兩層都留著是刻意的:這一層負責語意,toFixed 那一層負責「絕不產生 UB」。)
	if (!std::isfinite(qx) || !std::isfinite(qy) || !std::isfinite(qz) || !std::isfinite(qw)) return;

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
	// 取「這顆感測器自己的」加速度,不要拿到別顆的
	memcpy(&payload[9],  &p.accelFixed[sensorId][0], 2);
	memcpy(&payload[11], &p.accelFixed[sensorId][1], 2);
	memcpy(&payload[13], &p.accelFixed[sensorId][2], 2);

	PacketHandling::getInstance().insert(payload);
}

// ---- Battery(12):body@12 = voltage(f32 BE), percentage(f32 BE). len=20 ----
void SlimeServerEmu::handleBattery(Peer &p, const uint8_t *data, size_t len) {
	if (len < 20) return;
	float voltage = readBeFloat(&data[12]);     // volts
	float pct     = readBeFloat(&data[16]);     // 0..1 或 0..100(見下方註)
	// std::clamp(NaN, lo, hi) 會原封不動回傳 NaN(兩個比較都是 false),
	// 接著轉成整數就是 UB。
	// 這是純防禦性的:官方韌體的 BatteryMonitor 未讀到值時送的是 -1
	// (tracker-esp/src/batterymonitor.h:89 `float voltage = -1`),不是 NaN。
	// 但這四個 byte 是從 UDP 上照單全收的 32-bit 位元組合,任何韌體版本、
	// 任何損毀封包都可能讓它是 NaN/Inf —— 值域檢查不能建立在「對方會送什麼」上。
	// (-1 也一樣要處理:下面的 clamp 會把它夾成 0。)
	if (!std::isfinite(voltage)) voltage = 0.0f;
	if (!std::isfinite(pct)) pct = 0.0f;
	// 官方 percentage 是 0..1;轉成 0..100
	uint8_t pctU = static_cast<uint8_t>(std::clamp(pct <= 1.0f ? pct * 100.0f : pct, 0.0f, 100.0f));
	// 上限取 5.0V 而不是 6.5V:下游 setBattery() 存的是 (mv-2450)/10 的單一 byte,
	// 5.0V 剛好對到 255。用 6.5V 時 (6500-2450)/10 = 405 會溢位成 149,
	// server 上就變成「電池電壓 3.94V」這種看起來很合理卻完全錯誤的數字。
	uint16_t mv  = static_cast<uint16_t>(std::clamp(voltage, 0.0f, 5.0f) * 1000.0f);
	PacketHandling::getInstance().setBattery(p.trackerId, pctU, mv);
}

// ---- Temperature(20):body@12 = sensorId(1), temp(f32 BE). len=17 ----
void SlimeServerEmu::handleTemperature(Peer &p, const uint8_t *data, size_t len) {
	if (len < 17) return;
	uint8_t sensorId = data[12] & 0x0F;
	float tempC = readBeFloat(&data[13]);
	// 溫度要按 sensorId 分開存:一塊板子帶副 IMU 時主/副各送一包,
	// 共用一個欄位的話 server 上兩顆會顯示同一個溫度(後到的覆蓋先到的)。
	PacketHandling::getInstance().setTemp(p.trackerId, sensorId, encodeTemp(tempC));
}

// ---- SignalStrength(19):body@12 = sensorId(1), strength(int8). len=14 ----
void SlimeServerEmu::handleSignal(Peer &p, const uint8_t *data, size_t len) {
	if (len < 14) return;
	int8_t rssi = static_cast<int8_t>(data[13]);
	PacketHandling::getInstance().setRssi(p.trackerId, rssi);
}

// ---- SensorInfo(15):body@12 起。sensorId(1) sensorStatus(1) sensorType(1) ...
// 只取 sensorType 當 imuId(server 顯示 IMU 類型用);mag 先 0。 ----
void SlimeServerEmu::handleSensorInfo(Peer &p, const uint8_t *data, size_t len) {
	if (len < 15) return;
	// [12]sensorId [13]sensorStatus [14]sensorType
	uint8_t sensorId = data[12] & 0x0F;
	uint8_t imuId = data[14];
	// 同上:IMU 型號也是每顆感測器一份,副 IMU 常常和主 IMU 不同型號。
	PacketHandling::getInstance().setSensorInfo(p.trackerId, sensorId, imuId, 0);
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
		// connected 只由 update()(loop task)依 lastSeenMs 改寫,這裡不碰。
		// 兩個 task 各自寫同一個旗標時,「離線→上線」與「上線→離線」可能交錯:
		// update() 判定逾時的同時 onPacket 收到封包並設回 true,結果 status 封包
		// 送出的順序與最終狀態相反,PC 端就永遠停在錯的那一邊。
		m_peers[idx].lastSeenMs = millis();

		if (isNew && m_onConnected)
			m_onConnected(m_peers[idx].trackerId, m_peers[idx].mac);

		PacketHandling::getInstance().setFirmware(
			m_peers[idx].trackerId,
			static_cast<uint8_t>(boardType),
			static_cast<uint8_t>(mcuType),
			0, 0, 0, 0
		);
		PacketHandling::getInstance().setSensorInfo(
			m_peers[idx].trackerId,
			0,                                // handshake 只描述主感測器
			static_cast<uint8_t>(imuType),    // IMU 類型(=15)
			0                                 // mag 先 0
		);
		return;
	}

	int idx = findPeerByIp(pkt.remoteIP());
	if (idx < 0) return;
	Peer &p = m_peers[idx];

	// 這裡只留下時間戳。connected 的改寫與 setTrackerOnline() 一律交給 update()
	// (單一 writer,跑在 loop task),避免兩個 task 同時翻轉狀態。
	p.lastSeenMs = millis();

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

		// 時間比較用 int32_t 有號解讀:onPacket(lwip task)可能在本函式取完 now 之後
		// 才寫入 lastSeenMs/lastHeartbeatMs(值比 now 新)。若用 unsigned 減法會下溢成
		// 巨大正數 → 剛收到封包的追蹤器被瞬間誤判逾時。有號解讀時「未來」為負數,不誤觸發。
		// (感謝 mintocandy 回報此問題)

		// 先對帳狀態,再決定要不要發心跳 —— 順序反過來的話,剛逾時的 peer
		// 這一輪還會再收到一次心跳,把它的重新握手又往後推遲一個週期。
		// 下界也要擋。int32_t 解讀只在「差值很小」時才代表未來:差值一超過
		// 2^31 ms(約 24.85 天,這是有號解讀翻號的點,不是 millis() 的迴繞週期
		// —— millis() 是 uint32_t,週期為 2^32 ms 約 49.7 天),一顆早就沒在
		// 送封包的 peer 其 now-lastSeenMs 會被讀成很大的負數 → 只比上界的話會
		// 被當成「未來時間戳」而判定存活,離線的追蹤器就在 server 上永遠掛著。
		// 真正的跨 task 時間差只有幾毫秒,用同一個逾時值當下界綽綽有餘。
		//
		// 已知殘留(不修):差值掃過 2^32 的那一瞬間,sinceSeen 會從 -kTrackerTimeoutMs
		// 走到 +kTrackerTimeoutMs,這段窗口長 2×kTrackerTimeoutMs
		// (officialTrackerTimeoutMs 預設 6000ms → 12 秒),其間確實會誤判存活。
		// 前提是同一個 peer 槽連續 49.7 天沒收到任何封包;誤判會持續整整那 12 秒
		// (差值以真實時間往上爬,爬出 +kTrackerTimeoutMs 才恢復),之後自動歸位。
		// 要真正消掉得替每個 peer 多存一個「已判定離線」的黏著旗標,
		// 為 49.7 天一次、持續 12 秒的顯示抖動增加常駐狀態並不划算。
		int32_t sinceSeen = static_cast<int32_t>(now - p.lastSeenMs);
		bool alive = sinceSeen < static_cast<int32_t>(kTrackerTimeoutMs)
		          && sinceSeen > -static_cast<int32_t>(kTrackerTimeoutMs);
		if (alive != p.connected) {
			p.connected = alive;
			Serial.printf("[Emu] tracker id=%u -> %s\n",
				p.trackerId, alive ? "connected" : "timed out");
			PacketHandling::getInstance().setTrackerOnline(p.trackerId, alive);
		}

		// 心跳只發給「還在線且 IP 有效」的 peer。
		//  - IP 為 0.0.0.0 的是 DHCP 換手後被作廢的殘留 peer,發過去只是浪費。
		//  - 更重要的是:對已經逾時的追蹤器繼續發心跳,會讓它以為 server 還在,
		//    它就不會走重新 handshake 的流程 —— 於是一顆卡住的追蹤器永遠回不來,
		//    必須手動重開追蹤器電源。停發心跳後,追蹤器自己的逾時會觸發重新握手。
		// (用 !(a==b) 而非 a!=b:Arduino 的 IPAddress 只保證有 operator==)
		if (kEnableHeartbeat && p.connected && !(p.ip == IPAddress())
		    && static_cast<int32_t>(now - p.lastHeartbeatMs) >= static_cast<int32_t>(kHeartbeatIntervalMs)) {
			p.lastHeartbeatMs = now;
			sendHeartbeat(p);
		}
	}
}
