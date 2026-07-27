#pragma once

#include "HID.h"
#include "WifiDongleConfig.h"

#include <Arduino.h>
#include <cstdint>
#include <cstring>

// maxTrackers 的上限不是隨便寫的,改大會靜默壞掉,所以用 static_assert 擋住:
//  - HID device id 是 (sensorId<<4)|trackerId,trackerId 只有 4 bit → 必須 <= 15。
//  - Configuration 的 usedMask 是 16-bit,id >= 16 會被當成「沒用過」而重複配發。
//  - 補位封包(filler)用 device id 0xFF 當「不存在的裝置」,trackerId 到 15 就會撞。
//  - SoftAP 的 max_connection 本身也有上限,而且會隨 ESP-IDF 版本變動
//    (舊版 10、新版放寬到 ESP_WIFI_MAX_CONN_NUM=15)。取 10 是各版本都成立的保守值。
static_assert(WifiDongleConfig::maxTrackers >= 1 && WifiDongleConfig::maxTrackers <= 10,
              "WifiDongleConfig::maxTrackers must be between 1 and 10");

class PacketHandling {
public:
    static PacketHandling &getInstance();

    bool registerTracker(uint8_t trackerId, const uint8_t mac[6]);
    void insert(const uint8_t *payload);   // payload = 15 bytes
    void tick(HIDDevice &hidDevice);

    // ---- 官方相容:拿到就更新對應欄位(透明代理用)----
    void setBattery(uint8_t trackerId, uint8_t pct, uint16_t mv);     // type 12
    // 溫度與 IMU 型號是「每顆感測器各一份」:一塊板子帶副 IMU 時主/副各送一包,
    // 共用同一個欄位的話後到的會蓋掉先到的 → server 上副感測器顯示主感測器的型號與溫度。
    void setTemp(uint8_t trackerId, uint8_t sensorId, uint8_t tempEncoded);   // type 20
    void setRssi(uint8_t trackerId, int8_t rssi);                    // type 19
    void setSensorInfo(uint8_t trackerId, uint8_t sensorId, uint8_t imuId, uint8_t magId);  // type 15
    void setFirmware(uint8_t trackerId, uint8_t brdId, uint8_t mcuId,
                     uint16_t fwDate, uint8_t fwMajor, uint8_t fwMinor, uint8_t fwPatch); // handshake
    void setTrackerOnline(uint8_t trackerId, bool online);  // 斷線/重連:送 status + 停/復送 register

private:
    static constexpr size_t HID_PACKET_SIZE = 16;
    static constexpr size_t PACKETS_PER_REPORT = 4;
    static constexpr size_t HID_REPORT_SIZE = HID_PACKET_SIZE * PACKETS_PER_REPORT; // 64
    static constexpr size_t MAX_TRACKERS = 16;
    static constexpr size_t FIFO_SIZE = 64;
    static constexpr size_t PRIORITY_FIFO_SIZE = 32;   // status 等高優先封包。
    // 32 = 10 顆 tracker × 3 感測器餘裕:AP 層事件讓全部 tracker 同時斷線時,
    // 每顆的每個感測器各送一個 status,16 格會滿而靜默丟棄部分 status。
    static constexpr size_t MAX_SENSORS = 16;   // sensorId 為 4-bit(0..15)

    // 補位封包(type 254)的 device id。
    // server(DesktopHIDManager.kt)並沒有特別處理 254,它會照樣用 byte1 去查裝置;
    // 若留 0 就會查到 trackerId 0 的主感測器,而 HIDCommon.processPacket() 開頭有
    // 「TIMED_OUT → OK」的自動復活邏輯 → 0 號追蹤器永遠無法顯示成離線。
    // 0xFF 需要 trackerId=15,受上面的 static_assert 保證不可能存在 → server 查不到就跳過。
    static constexpr uint8_t HID_FILLER_ID = 0xFF;

    struct TrackerInfo {
        bool used = false;
        bool online = true;     // 斷線時設 false → 停送 register/device_info
        uint8_t id = 0;
        uint16_t sensorMask = 1;  // 此 tracker 出現過的 sensorId(bit0=主感測器,bit1..=副追蹤器)
        uint8_t mac[6] = {0};
        uint8_t batt = 0;
        uint8_t battV = 0;
        int8_t rssi = 0;
        bool hasBattData = false;
        uint8_t brdId = 0;
        uint8_t mcuId = 0;
        // 每顆感測器各自的溫度與 IMU/磁力計型號(index = sensorId)
        uint8_t temp[MAX_SENSORS] = {};
        uint8_t imuId[MAX_SENSORS] = {};
        uint8_t magId[MAX_SENSORS] = {};
        // 哪些 sensorId 已經收到過真正的 SensorInfo。
        // 不能只看 imuId[s] != 0:0 本身也可能是伺服器認得的型號,而且更重要的是
        // 「還沒收到」與「收到了但值是 0」必須分得開 —— server 端 Tracker.imuType
        // 是 val,第一包 device_info 帶什麼型號就永久定型,整個 session 都改不掉。
        uint16_t imuKnownMask = 0;
        // 每顆感測器第一次出現的時間(用來做「等 SensorInfo」的寬限期)
        uint32_t sensorSeenMs[MAX_SENSORS] = {};
        // 尚未拿到自己的 SensorInfo 時的後備型號:握手封包裡的主 IMU 型號。
        // 副感測器多半和主感測器同型號,就算不同,顯示成主感測器的型號也遠比
        // 顯示成 Unknown 有用 —— 後者在 server 上會讓校正/濾波的型號相依邏輯失準。
        uint8_t defaultImuId = 0;
        uint8_t defaultMagId = 0;
        uint16_t fwDate = 0;
        uint8_t fwMajor = 0;
        uint8_t fwMinor = 0;
        uint8_t fwPatch = 0;
        // 這一輪(episode)裡,這個槽被「換上另一顆 MAC」而重新計時寬限期的次數。
        // 用來擋住寬限期被無限重新計時:兩顆追蹤器若拿到同一個 trackerId
        // (配對表損壞、或某顆 flash 裡留著別台 dongle 配的舊 id),它們會輪流
        // registerTracker() 進同一個槽,每一次都把 sensorSeenMs[0] 重設 →
        // 寬限期永遠不會到期 → 這顆在 server 上永遠不出現,而序列埠上只看得到
        // 它一直重連。額度用完就不再重新計時,讓它以 UNKNOWN 型號出現 ——
        // 出現但型號不明,遠好過完全不出現(server 對沒註冊過的 sensorId 會
        // 直接丟棄所有資料封包,見 HIDCommon.kt:169-172)。
        //
        // 「一輪」是用時間切的,不是用槽位是否空著切的:見 lastGraceArmMs。
        //
        // 值域是 0..MAX_GRACE_REARMS+1,MAX+1 是吸收態(到頂就不再往上加)。
        // 兩邊的判斷必須對得起來:registerTracker() 在「加之前」比 < MAX,
        // 也就是前 MAX 次配對每一次都真的拿到一份完整的寬限期;因此 tick()
        // 在「加之後」的值上要判 rearms > MAX_GRACE_REARMS 才算額度用完。
        // 寫成 >= MAX 的話,第 MAX 次配對明明剛拿到寬限期,tick 卻已經當它用完,
        // 而且那一次不會印任何 log(log 在第 MAX+1 次才觸發)—— 於是那顆感測器
        // 會無聲地以 UNKNOWN 註冊,常數名也變成謊話(實際只買到 MAX-1 個窗口)。
        // MAX+1 同時是「這一輪的耗盡 log 已經印過」的標記:乒乓可能每秒發生一次,
        // 不加標記就會洗版。
        //
        // 另外,「用完」本身是有時效的:判斷時必須連 lastGraceArmMs 一起看
        // (見 tick() 的 episodeLive)。沒有任何地方會隨時間把這個計數降回去,
        // 只看它的話乒乓結束後這個槽的寬限期就永久失效了。
        uint8_t graceRearms = 0;
        // 上一次重新計時寬限期的時刻。距離它超過 GRACE_EPISODE_MS 就視為新的一輪,
        // graceRearms 歸零。不能改用「槽位本來是空的」當歸零條件 ——
        // 這棵樹裡沒有任何地方把 used 設回 false,槽位一旦用過就永遠 used=true,
        // 那個條件實際上永遠不成立,額度會退化成「開機以來共 MAX_GRACE_REARMS 次」。
        // 後果:一台正常使用的 dongle 只要重新配對過幾次,之後每一次全新配對都
        // 拿不到寬限期,tick() 會立刻以 imuId=0 註冊 —— 而 IMUType.getById(0) 是
        // 合法的 UNKNOWN(FirmwareConstants.kt:3-6)、Tracker.imuType 是 val
        // (Tracker.kt:61),那顆感測器整個 session 的型號就永久停在 Unknown。
        uint32_t lastGraceArmMs = 0;
    };

    struct Packet { uint8_t data[HID_PACKET_SIZE]; };

    PacketHandling() = default;
    static PacketHandling instance;

    TrackerInfo trackers[MAX_TRACKERS];

    // FIFO（環形）
    Packet fifo[FIFO_SIZE];
    size_t fifoHead = 0;
    size_t fifoTail = 0;
    bool fifoFull = false;

    // 高優先 FIFO:斷線 status 等不可丟失的封包,tick 會優先送
    Packet priorityFifo[PRIORITY_FIFO_SIZE];
    size_t priorityHead = 0;
    size_t priorityTail = 0;
    bool priorityFull = false;
    // tail 每前進一格就 +1,永不回頭。priorityPeek/priorityDrop 之間用它判斷
    // 「這期間 tail 有沒有被動過」。不能直接比 priorityTail 的值:那是 mod 32 的
    // 環形索引,peek 與 drop 之間若剛好被 push 擠掉整整 32 筆,tail 會繞回同一個
    // 數值,比較結果是「沒變」,於是把 32 筆全新的狀態當成剛送出的那批丟掉(ABA)。
    // 佇列只有 32 格,全部追蹤器同時斷線再同時回來就足以在一次 send() 內填滿。
    uint32_t priorityTailSeq = 0;

    // 每 100ms 那一輪 register/device_info 的起點。send() 失敗會中斷整輪,
    // 若每輪都從 0 開始,清單後段的追蹤器在「USB 半通不通」時永遠輪不到 ——
    // 它們在 server 上就是不存在。失敗時把起點往後推,讓輪替涵蓋所有人。
    size_t regRotateIndex = 0;
    uint32_t lastRegSentMs = 0;

    // 高優先佇列滿而被壓縮掉的舊狀態筆數(只用於序列埠診斷)
    uint32_t priorityDropped = 0;
    uint32_t priorityDroppedLogged = 0;

    // 某個 sensorId 第一次出現後,最多等這麼久它自己的型號資訊。
    // 等不到就用 defaultImuId 先註冊,絕不能無限期不註冊 ——
    // 無限期等待會讓那顆感測器在 server 上永遠不出現。
    //
    // 型號來自 SensorInfo(type 15),每顆感測器各一包,但送出時機沒有保證,
    // 且副感測器的 RotationData 通常比它的 SensorInfo 早到 ——
    // 這個寬限期就是為這一刻存在的。
    static constexpr uint32_t SENSOR_INFO_GRACE_MS = 2000;

    // 同一輪裡最多允許重新計時寬限期幾次(見 TrackerInfo::graceRearms)。
    static constexpr uint8_t MAX_GRACE_REARMS = 5;

    // 兩次重新配對相隔超過這麼久,就當作是新的一輪,額度歸零。
    // 取 10 秒的理由:要擋的乒乓是「兩顆追蹤器搶同一個 id、輪流重連」,
    // 而追蹤器的重連間隔是秒等級(連不上就重試),必定遠短於 10 秒;
    // 反過來,使用者手動重新配對一顆追蹤器至少要按鈕、等開機,兩次之間
    // 隔上 10 秒以上是常態,額度會在中間歸零。這條界線把兩種情況分得開。
    //
    // 注意這是「滑動視窗」而不是固定視窗:lastGraceArmMs 每次重新配對都會更新
    // (額度用完的那條路徑也會更新),所以判斷的是「相鄰兩次的間隔」,
    // 不是「最近 10 秒內幾次」。連續每 9 秒重新配對一次,第 6 次一樣會用完額度 ——
    // 這是刻意的:那種頻率本身就代表有東西在反覆搶同一個 id。
    static constexpr uint32_t GRACE_EPISODE_MS = 10000;

    // 跨 task spinlock:寫入(lwip task 的 onPacket->insert)與讀出(loop task 的 tick)
    // 同時操作 FIFO,必須鎖保護,否則高流量下指標競爭會掉包/亂序。
    portMUX_TYPE m_mux = portMUX_INITIALIZER_UNLOCKED;

    int findTracker(uint8_t id);
    // 以 HID device id(=(sensorId<<4)|trackerId)送 status 封包
    void pushStatusHid(uint8_t hidId, uint8_t status);
    bool fifoEmpty() const { return (fifoHead == fifoTail) && !fifoFull; }
    void fifoPush(const Packet &p, uint8_t hidId);
    bool fifoPop(Packet &out);

    // 高優先 FIFO 操作。
    // 注意這裡是「先看再丟」而不是 pop:status 封包不可遺失,但 hidDevice.send() 可能失敗
    // (主機沒在輪詢 USB)。先 pop 再 send 的話,送失敗那批就永遠消失了 ——
    // 而 setTrackerOnline() 只在狀態「改變」時才推送,不會再補一次,
    // 於是 PC 端會停在錯誤狀態(例如追蹤器已離線卻永遠顯示連線中)。
    bool priorityEmpty() const { return (priorityHead == priorityTail) && !priorityFull; }
    void priorityPush(const Packet &p);
    // 讀第 offset 筆,不移除;tailSeen 回報當下的 tail 序號,必須原封不動傳給 priorityDrop
    bool priorityPeek(size_t offset, Packet &out, uint32_t &tailSeen);
    // 確認送出後才移除前 count 筆;序號與 peek 當時不同就整批不移除(見 .cpp 說明)
    void priorityDrop(size_t count, uint32_t tailSeen);
};
