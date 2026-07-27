#include "packetHandling.h"

PacketHandling &PacketHandling::getInstance() {
    return instance;
}

int PacketHandling::findTracker(uint8_t id) {
    for (size_t i = 0; i < MAX_TRACKERS; i++) {
        if (trackers[i].used && trackers[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool PacketHandling::registerTracker(uint8_t trackerId, const uint8_t mac[6]) {
    // trackerId 是外部給的值,不能假設一定在範圍內(配對表損壞、或未來新增的
    // 呼叫端傳入未檢查的值都有可能)。放行的後果不只是「多佔一個槽」:
    //  - id=15 且 sensorId=15 時 hidId=0xFF —— 那正是補位封包(type 254)用來表示
    //    「不存在的裝置」的 id。server 於是真的建出 0xFF 這個裝置,之後每一個補位
    //    封包都會餵到它身上並觸發 HIDCommon 的 TIMED_OUT→OK,它再也不可能顯示離線。
    //  - id >= 16 時 (sensorId<<4)|id 是 OR 而不是拼接,不同 sensorId 會疊出同一個
    //    hidId;pushStatusHid() 的 (hidId & 0x0F) 更會把狀態算到「別顆」追蹤器頭上。
    // packetHandling.h 那條 static_assert 只約束 Configuration 的配發範圍,
    // 擋不住從網路進來的值 —— 必須在這裡擋。
    if (trackerId >= WifiDongleConfig::maxTrackers) {
        Serial.printf("[HID] rejecting out-of-range trackerId %u\n",
                      static_cast<unsigned>(trackerId));
        return false;
    }

    int idx = findTracker(trackerId);
    bool isNew = (idx < 0);
    if (idx < 0) {
        for (size_t i = 0; i < MAX_TRACKERS; i++) {
            if (!trackers[i].used) { idx = static_cast<int>(i); break; }
        }
    }
    if (idx < 0) return false;

    // 判斷「這個槽換了一顆追蹤器」不能只看 isNew。
    // isNew 的意思是「這個 trackerId 之前不在表上」,但同一個 trackerId 也可能
    // 被指派給不同的 MAC:五連按清空配對表(若沒接著重開機)、或配對表損壞後
    // 重配 id,都會讓既有槽位上的 MAC 換人。此時 isNew=false,舊追蹤器的
    // imuId/imuKnownMask/sensorMask 會原封不動留給新追蹤器 ——
    // 新的那顆於是頂著前一顆的 IMU 型號與副感測器清單在 server 上出現。
    // 用 MAC 是否改變來判斷才對得上「換人了」這件事本身。
    //
    // 要說清楚這個清除能做到什麼、不能做到什麼:
    //  - 不能做到的:server 端是用 HID id 當 key 的,同一個 id 一旦註冊過,
    //    裝置名稱與 Tracker.imuType(val)就固定了,dongle 這邊怎麼清都改不回來。
    //    真正要換型號,使用者得重開 server(或重開 dongle 讓 id 重新分配)。
    //  - 能做到的:(a) server 還沒註冊過這個 id 時(dongle 先換人、server 後開,
    //    或這是一顆新的 sensorId),送出去的就是正確的型號,而不是繼承來的舊型號;
    //    (b) 電量/電壓/溫度/RSSI 這些欄位 server 每包 device_info 都會更新,
    //    清掉就不會有「新追蹤器顯示前一顆電量」的空窗;
    //    (c) online 若留著前一顆的 false,tick() 會直接跳過這個槽,新的那顆
    //    連一包 register 都不會送出 —— 這一項是致命的,而且清除確實能修好它。
    bool freshSlot = isNew || memcmp(trackers[idx].mac, mac, 6) != 0;

    // 先填完內容、最後才設 used=true:tick()(loop task,另一個 core)隨時在讀這個陣列。
    // 若先設 used 再填 MAC,tick 可能搶在中間送出「MAC 只寫一半」的 register 封包;
    // server 會用這個亂碼位址建立裝置,且該次 session 內裝置名稱不會再更新(沾黏)。
    //
    // 但「最後才設 used」只保護第一次註冊。同一個槽換人(freshSlot 且 used 早就是
    // true)時 tick 一直在讀 mac[],6 byte 的 memcpy 不是原子操作 —— tick 可能讀到
    // 前 3 byte 是新的、後 3 byte 是舊的組合,一樣送出亂碼位址。所以 id 與 mac 也要
    // 進臨界區,tick() 端則在同一把鎖下把 id/mac 一起抓成快照。
    // millis() 先在鎖外取:臨界區裡只做記憶體搬移,不呼叫任何可能被中斷/阻塞的東西。
    uint32_t nowMs = millis();
    bool budgetExhausted = false;

    portENTER_CRITICAL(&m_mux);
    trackers[idx].id = trackerId;
    memcpy(trackers[idx].mac, mac, 6);
    if (freshSlot) {
        // 這個槽可能是上一顆追蹤器用過的 —— 只清型號是不夠的。
        // 電量/電壓/溫度/RSSI/韌體版本若留著上一顆的值,新的那顆在 server 上
        // 一進來就頂著別人的電量與版本號顯示,而且只有在對應封包再次到達時才會
        // 更正(RSSI 與溫度可能要等好幾秒,hasBattData 則可能永遠不再更新)。
        // online 更關鍵:上一顆若是以 TIMED_OUT 收場,online 會留在 false,
        // 而 tick() 對 !online 的槽直接 continue —— 新配對上來的追蹤器於是
        // 完全不送 register/device_info,在 server 上永遠不出現,序列埠也不報錯。
        // 額度是「每一輪」各自計算的,一輪用時間切:距離上一次重新計時
        // 超過 GRACE_EPISODE_MS 就當作新的一輪。理由見 TrackerInfo::lastGraceArmMs
        //(簡言之:沒有任何地方把 used 設回 false,拿槽位是否空著當條件會永遠不成立)。
        // 用有號差值,millis() 迴繞與跨 task 取樣造成的「時間戳在未來」都能正確處理:
        // 未來的時間戳算出負值 → 一樣視為新的一輪,給滿額度,絕不會反而少等。
        int32_t sinceLastArm = static_cast<int32_t>(nowMs - trackers[idx].lastGraceArmMs);
        if (sinceLastArm < 0 || sinceLastArm >= static_cast<int32_t>(GRACE_EPISODE_MS)) {
            trackers[idx].graceRearms = 0;
        }
        trackers[idx].lastGraceArmMs = nowMs;
        trackers[idx].online = true;
        trackers[idx].sensorMask = 1;   // 新註冊:先只有主感測器,副追蹤器等資料進來再補
        trackers[idx].imuKnownMask = 0;
        trackers[idx].defaultImuId = 0;
        trackers[idx].defaultMagId = 0;
        memset(trackers[idx].imuId, 0, sizeof(trackers[idx].imuId));
        memset(trackers[idx].magId, 0, sizeof(trackers[idx].magId));
        memset(trackers[idx].temp, 0, sizeof(trackers[idx].temp));
        trackers[idx].batt = 0;
        trackers[idx].battV = 0;
        trackers[idx].rssi = 0;
        trackers[idx].hasBattData = false;
        trackers[idx].brdId = 0;
        trackers[idx].mcuId = 0;
        trackers[idx].fwDate = 0;
        trackers[idx].fwMajor = 0;
        trackers[idx].fwMinor = 0;
        trackers[idx].fwPatch = 0;
        // 主感測器的寬限期從現在起算 —— 但同一輪裡重新計時的次數有上限。
        // 沒有上限的話,兩顆搶同一個 trackerId 的追蹤器會輪流把它推遲,
        // 兩顆都永遠等不到註冊(見 TrackerInfo::graceRearms)。
        if (trackers[idx].graceRearms < MAX_GRACE_REARMS) {
            // 前 MAX_GRACE_REARMS 次重新配對,每一次都給一份完整的寬限期。
            trackers[idx].graceRearms++;
            trackers[idx].sensorSeenMs[0] = nowMs;
        } else if (trackers[idx].graceRearms == MAX_GRACE_REARMS) {
            // 第 MAX+1 次 —— 額度就是在這一次用完的。把值推到 MAX+1,
            // 它同時是「額度已用完」的判斷值(tick() 用 rearms > MAX_GRACE_REARMS)
            // 與「已經記過 log」的標記:乒乓可能每秒發生一次,每次都印會洗版。
            // 之後不再往上加,MAX+1 是穩定的吸收態。
            //
            // 這裡的比較用「加之前的值」,tick() 用「加之後的值」,兩邊必須對得起來:
            // 寫成 tick() 判 rearms >= MAX 的話,第 MAX 次配對明明才剛把
            // sensorSeenMs[0] 重設(上面那個分支),tick 卻已經認定額度用完而完全
            // 不看那個時間戳 —— 那一次的寬限期是白給的,而且序列埠上不會有任何訊息
            //(log 要到第 MAX+1 次才印),使用者只會看到型號莫名其妙變成 Unknown。
            trackers[idx].graceRearms++;
            budgetExhausted = true;
        }
        // 額度用完之後就完全不等了(見 tick()):下一次 tick 直接以 defaultImuId
        // 註冊出去,主感測器的話那個值必然是 0,也就是 UNKNOWN。
        // 這是刻意的取捨,而且要說清楚它的代價:
        //   - 主感測器(s==0)的 defaultImuId 與 imuKnownMask 是同時被設定的,
        //     所以 !imuKnown 時 defaultImuId 必然是 0 —— 這條路徑對主感測器
        //     而言就是「以 UNKNOWN 註冊」,不是「以主感測器型號註冊」。
        //     IMUType.getById(0) 會回傳合法的 UNKNOWN(FirmwareConstants.kt:3-6),
        //     而 Tracker.imuType 是 val(Tracker.kt:61)→ 整個 session 都改不掉。
        //   - 但另一邊更糟:沒註冊過的 sensorId,server 在 HIDCommon.kt:169-172
        //     會直接 return,連旋轉資料都丟掉,那顆追蹤器完全不能用。
        // 型號不明但能動 > 完全不出現,所以選前者。
    }
    trackers[idx].used = true;                 // 最後設,確保 tick 看到 used 時內容已完整
    portEXIT_CRITICAL(&m_mux);

    // 序列埠輸出會阻塞,一定要出了臨界區才做(spinlock 內禁止任何可能等待的呼叫)。
    if (budgetExhausted) {
        Serial.printf("[HID] tracker %u re-paired %u times in a row (< %lu ms apart); no longer waiting "
                      "for sensor info, will register with unknown IMU type (two trackers sharing an id?)\n",
                      static_cast<unsigned>(trackerId),
                      static_cast<unsigned>(MAX_GRACE_REARMS) + 1u,
                      static_cast<unsigned long>(GRACE_EPISODE_MS));
    }
    return isNew;
}

// ---- 官方相容 setter:拿到就更新對應欄位 ----
void PacketHandling::setBattery(uint8_t trackerId, uint8_t pct, uint16_t mv) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    // battV 只有 1 byte,編碼是 (mv-2450)/10 → mv 超過 5000 就會溢位,
    // 而且溢位後看起來仍然像個合理電壓(例如 6.5V 會顯示成 3.94V)。
    // 上游(SlimeServerEmu 的 handleBattery)夾的是 6.5V,不是 5.0V ——
    // 也就是上游允許的範圍本來就會讓這裡溢位。本補丁把那邊改夾 5.0V,
    // 這裡則不依賴呼叫端:setBattery 是 public,飽和運算在這裡再做一次。
    uint16_t enc = (mv > 2450) ? static_cast<uint16_t>((mv - 2450) / 10) : 0;
    uint8_t encV = static_cast<uint8_t>(enc > 255 ? 255 : enc);
    // 百分比與電壓成對才有意義(server 上兩個數字並排顯示),一起寫進去。
    // 換算刻意留在鎖外做:臨界區裡只放記憶體搬移。
    portENTER_CRITICAL(&m_mux);
    trackers[idx].batt = pct;
    trackers[idx].battV = encV;
    trackers[idx].hasBattData = true;
    portEXIT_CRITICAL(&m_mux);
}

// 以下兩個刻意不上鎖:各自只寫一個 byte,沒有
// 「多個欄位要一起生效」的問題,tick() 讀到的不是新值就是舊值,兩者都合理。
// 上鎖反而讓 lwIP task 多付一次關中斷的代價。
void PacketHandling::setTemp(uint8_t trackerId, uint8_t sensorId, uint8_t tempEncoded) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    trackers[idx].temp[sensorId & 0x0F] = tempEncoded;   // & 0x0F:陣列大小 16,索引不可能越界
}

void PacketHandling::setRssi(uint8_t trackerId, int8_t rssi) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    trackers[idx].rssi = rssi;
}

void PacketHandling::setSensorInfo(uint8_t trackerId, uint8_t sensorId, uint8_t imuId, uint8_t magId) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    uint8_t s = sensorId & 0x0F;
    // 值與「已知」旗標要一起讓 tick() 看見,否則 tick 可能看到旗標已設、
    // 值卻還是舊的(見 tick() 的快照說明)。用同一把 spinlock 綁成臨界區。
    portENTER_CRITICAL(&m_mux);
    trackers[idx].imuId[s] = imuId;
    trackers[idx].magId[s] = magId;
    // 主感測器的型號同時當作其他感測器的後備值。握手封包只描述主 IMU,
    // 但副感測器的 RotationData 會比它自己的 SensorInfo 先到。原因不是主迴圈的
    // 順序(官方韌體 main.cpp:181 其實是先 networkManager.update() 再
    // sensorManager.update()),而是 Connection::updateSensorState() 裡的節流:
    // 它每秒才檢查一次要不要送 SensorInfo(connection.cpp:504-516),
    // 而 RotationData 是每次取樣就送。沒有後備值的話那一刻只能送 imuId=0,
    // 而 Tracker.imuType 在 server 端是 val —— 第一包 device_info 帶什麼就定型,
    // 之後再送正確型號也不會被採用。
    if (s == 0) {
        trackers[idx].defaultImuId = imuId;
        trackers[idx].defaultMagId = magId;
    }
    trackers[idx].imuKnownMask |= static_cast<uint16_t>(1u << s);
    portEXIT_CRITICAL(&m_mux);
}

void PacketHandling::setFirmware(uint8_t trackerId, uint8_t brdId, uint8_t mcuId,
                                 uint16_t fwDate, uint8_t fwMajor, uint8_t fwMinor, uint8_t fwPatch) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    // 六個欄位要一起讓 tick() 看見。tick() 是在另一顆 core 上用同一把鎖抓快照的,
    // 這邊不上鎖的話它可能拿到「新的 fwDate + 舊的 fwMajor/Minor/Patch」,
    // 送出去就是一個從來不存在的版本號組合。
    //(下一輪 100ms 後會自己修正,但版本號是使用者回報問題時唯一的依據,
    // 顯示成不存在的版本會直接把診斷帶偏。)
    portENTER_CRITICAL(&m_mux);
    trackers[idx].brdId = brdId;
    trackers[idx].mcuId = mcuId;
    trackers[idx].fwDate = fwDate;
    trackers[idx].fwMajor = fwMajor;
    trackers[idx].fwMinor = fwMinor;
    trackers[idx].fwPatch = fwPatch;
    portEXIT_CRITICAL(&m_mux);
}

void PacketHandling::pushStatusHid(uint8_t hidId, uint8_t status) {
    Packet p;
    memset(p.data, 0, HID_PACKET_SIZE);
    p.data[0] = 3;
    p.data[1] = hidId;
    p.data[2] = status;
    // status 封包也要帶 rssi(byte15),否則 server 會把 signalStrength 洗成 0。
    // packets_received/lost、windows_hit/missed(byte4~7)目前沒有統計來源,先留 0。
    int idx = findTracker(static_cast<uint8_t>(hidId & 0x0F));
    if (idx >= 0) {
        int8_t r = trackers[idx].rssi;
        p.data[15] = static_cast<uint8_t>(r < 0 ? -r : r);
    }
    priorityPush(p);
}

void PacketHandling::setTrackerOnline(uint8_t trackerId, bool online) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;

    // check-and-set 需加鎖:逾時判定(loop task)與資料復活(lwip task)可能同時呼叫,
    // 不加鎖時兩邊都可能通過「狀態沒變」檢查 → 重複送 status 或遺失一次轉換。
    // 注意 status 封包要在鎖外送(priorityPush 內部會取同一把 spinlock,不可重入)。
    // sensorMask 也在同一個臨界區裡抓走:它由 lwip task 的 insert() 更新,
    // 底下的迴圈若邊跑邊讀,新出現的副感測器可能剛好在迴圈跑過它之後才被設進
    // mask —— 那顆感測器就漏掉這次的狀態封包,而 setTrackerOnline() 只在狀態
    // 「改變」時推送,不會補送 → 它在 server 上永遠停在錯誤的連線狀態。
    portENTER_CRITICAL(&m_mux);
    bool changed = (trackers[idx].online != online);
    if (changed) trackers[idx].online = online;
    uint16_t sensorMask = trackers[idx].sensorMask;
    portEXIT_CRITICAL(&m_mux);
    if (!changed) return;   // 狀態沒變,不重複處理

    // 對這顆 tracker 的每個感測器(主+副)都送狀態封包。
    // 離線時送 TIMED_OUT(5) 而非 DISCONNECTED(0):server 只會在 TIMED_OUT 狀態下
    // 讓「進來的資料」自動把狀態救回 OK(HIDCommon.kt),DISCONNECTED 則要等明確的
    // status=OK 才會恢復。用 TIMED_OUT 可避免「顯示離線、資料卻一直進來」卡住。
    for (uint8_t s = 0; s < MAX_SENSORS; s++) {
        if (!(sensorMask & (1u << s))) continue;
        uint8_t hid = static_cast<uint8_t>((s << 4) | trackerId);
        pushStatusHid(hid, online ? 1 : 5);
    }

    if (online) {
        lastRegSentMs = 0;
        Serial.printf("[HID] tracker %u back ONLINE\n", trackerId);
    } else {
        Serial.printf("[HID] tracker %u marked TIMED_OUT\n", trackerId);
    }
}

void PacketHandling::fifoPush(const Packet &p, uint8_t hidId) {
    portENTER_CRITICAL(&m_mux);
    // 去重:同一感測器(hidId=(sensorId<<4)|trackerId)的資料封包(data[0]==1)已在佇列就原地更新。
    //
    // 這裡必須是 do-while 而不是 while。佇列滿的時候 head == tail,
    // 前置判斷式的 while 迴圈一次都不會跑 → 完全不去重 → 接著撞上底下的
    // 「滿了就丟」而把這包丟掉。而佇列滿正是最需要去重的時候:那一刻幾乎
    // 一定有同一顆感測器的舊姿態還躺在佇列裡,原地更新是零成本的,丟掉卻讓
    // 那顆感測器的畫面停格。(fifoEmpty() 已在外面擋掉「空」的情況,
    // 所以進到這裡時 head == tail 只可能代表滿。)
    if (!fifoEmpty()) {
        size_t idx = fifoTail;
        do {
            if (fifo[idx].data[1] == hidId && fifo[idx].data[0] == 1) {
                fifo[idx] = p;
                portEXIT_CRITICAL(&m_mux);
                return;
            }
            idx = (idx + 1) % FIFO_SIZE;
        } while (idx != fifoHead);
    }
    if (fifoFull) {
        portEXIT_CRITICAL(&m_mux);
        return;
    }
    fifo[fifoHead] = p;
    fifoHead = (fifoHead + 1) % FIFO_SIZE;
    if (fifoHead == fifoTail) fifoFull = true;
    portEXIT_CRITICAL(&m_mux);
}

bool PacketHandling::fifoPop(Packet &out) {
    portENTER_CRITICAL(&m_mux);
    if (fifoEmpty()) {
        portEXIT_CRITICAL(&m_mux);
        return false;
    }
    out = fifo[fifoTail];
    fifoTail = (fifoTail + 1) % FIFO_SIZE;
    fifoFull = false;
    portEXIT_CRITICAL(&m_mux);
    return true;
}

void PacketHandling::priorityPush(const Packet &p) {
    portENTER_CRITICAL(&m_mux);
    if (priorityFull) {
        // 滿了要丟「最舊的」而不是拒收最新的。
        // 這個佇列裝的是狀態轉換(上線/離線),PC 端看到的是最後一筆的結果。
        // 拒收最新的話,丟掉的正好是「現在的真實狀態」,佇列裡留下的是一堆過期轉換 ——
        // 例如全部追蹤器同時離線又陸續回來時,server 會停在「離線」而資料一直進來。
        // 丟最舊的則相反:最新 32 筆一定涵蓋每顆追蹤器(最多 10 顆)的最後狀態,
        // 中間被壓縮掉的只是使用者本來也看不到的中間過程。
        priorityTail = (priorityTail + 1) % PRIORITY_FIFO_SIZE;
        priorityTailSeq++;              // tail 動了 → 讓進行中的 peek/drop 失效
        priorityFull = false;
        priorityDropped++;
    }
    priorityFifo[priorityHead] = p;
    priorityHead = (priorityHead + 1) % PRIORITY_FIFO_SIZE;
    if (priorityHead == priorityTail) priorityFull = true;
    portEXIT_CRITICAL(&m_mux);
}

// 讀出佇列中第 offset 筆(不移除)。offset=0 是最舊的一筆。
// tailSeen 回報當下的 tail 位置,呼叫端要原封不動傳給 priorityDrop() ——
// 理由見 priorityDrop()。
bool PacketHandling::priorityPeek(size_t offset, Packet &out, uint32_t &tailSeen) {
    portENTER_CRITICAL(&m_mux);
    size_t count = priorityFull
        ? PRIORITY_FIFO_SIZE
        : ((priorityHead + PRIORITY_FIFO_SIZE - priorityTail) % PRIORITY_FIFO_SIZE);
    tailSeen = priorityTailSeq;
    if (offset >= count) {
        portEXIT_CRITICAL(&m_mux);
        return false;
    }
    out = priorityFifo[(priorityTail + offset) % PRIORITY_FIFO_SIZE];
    portEXIT_CRITICAL(&m_mux);
    return true;
}

// 確認 HID 送出成功後才真正移除。送失敗就不呼叫,下一輪 tick 會重送同一批。
//
// tailSeen 是 peek 當下的 tail「序號」。peek 與 drop 之間隔著一次 hidDevice.send()
// (可能耗時數十毫秒),這段期間 lwIP task 可以 priorityPush();佇列滿時
// push 會「丟最舊的」而把 tail 往前推。若這時仍照 count 盲目推進 tail,
// 推掉的就不是剛送出去的那幾筆,而是後面那幾筆還沒送出的新狀態 ——
// 那正是這個佇列存在的理由(status 不可遺失)。
// 序號對不上就什麼都不丟:被 push 擠掉的那幾筆本來就已經不在佇列裡,
// 沒有東西需要移除;還留著的是更新的狀態,下一輪 tick 會送。
// 代價只是那幾筆可能被重送一次,而重送 status 對 server 是等冪的。
//
// 比的是單調遞增的 priorityTailSeq 而不是 priorityTail 本身。後者是 mod 32 的
// 環形索引:peek 與 drop 之間若剛好被擠掉整整 32 筆,它會繞回同一個數值,
// 檢查會誤判成「沒人動過」,於是把 32 筆全新的狀態當成剛送出的那批丟掉。
// 32 格聽起來很多,但 10 顆追蹤器×3 感測器同時斷線再同時回來就是 60 筆。
void PacketHandling::priorityDrop(size_t count, uint32_t tailSeen) {
    if (count == 0) return;
    portENTER_CRITICAL(&m_mux);
    if (priorityTailSeq != tailSeen) {
        portEXIT_CRITICAL(&m_mux);
        return;
    }
    size_t avail = priorityFull
        ? PRIORITY_FIFO_SIZE
        : ((priorityHead + PRIORITY_FIFO_SIZE - priorityTail) % PRIORITY_FIFO_SIZE);
    if (count > avail) count = avail;
    priorityTail = (priorityTail + count) % PRIORITY_FIFO_SIZE;
    priorityTailSeq += static_cast<uint32_t>(count);
    if (count > 0) priorityFull = false;
    portEXIT_CRITICAL(&m_mux);
}

void PacketHandling::insert(const uint8_t *payload) {
    uint8_t trackerId = payload[0] >> 4;
    uint8_t sensorId  = payload[0] & 0x0F;   // 副追蹤器:主感測器=0,副感測器=1..

    int idx = findTracker(trackerId);
    if (idx < 0) return;

    // 第一次看到這個 sensorId → 記錄下來,並立刻補送 register/device_info,
    // 讓 server 把它註冊成一顆獨立的 tracker(否則資料會被 server 丟棄)。
    if (!(trackers[idx].sensorMask & (1u << sensorId))) {
        // 時間戳與 mask 必須成對地讓 tick() 看見。tick() 跑在另一個 task/core,
        // 它一看到 mask 上的位元就會拿同一格 sensorSeenMs 算寬限期;
        // 若它讀到「mask 已設、時間戳還是 0」,寬限期當場算成早已過期,
        // 於是立刻送出 imuId=0 的 device_info —— 正好是這整套機制要避免的事,
        // 而且 server 端 Tracker.imuType 是 val,錯了就整個 session 改不回來。
        // 只靠「先寫時間戳再寫 mask」的程式碼順序是不夠的:編譯器與 CPU 都可以
        // 重排這兩個沒有相依性的寫入。這裡用和 FIFO 同一把 spinlock 把兩個寫入
        // 綁成一個臨界區,tick() 端也在同一把鎖下把兩者一起讀走(見 tick())。
        // millis() 在鎖外先取:臨界區裡只做記憶體搬移。
        // (esp_timer 自己也有鎖,巢狀持鎖沒必要,而且會拉長關中斷的時間。)
        uint32_t seenMs = millis();
        portENTER_CRITICAL(&m_mux);
        trackers[idx].sensorSeenMs[sensorId] = seenMs;
        trackers[idx].sensorMask |= (1u << sensorId);
        portEXIT_CRITICAL(&m_mux);
        lastRegSentMs = 0;
    }

    // HID device id 同時編碼 trackerId 與 sensorId:
    //   (sensorId<<4)|trackerId → 主感測器(s=0)= trackerId(與舊版相容),
    //   副感測器則落在不同 id,不會再互相覆蓋。
    uint8_t hid = static_cast<uint8_t>((sensorId << 4) | trackerId);

    Packet p;
    memset(p.data, 0, HID_PACKET_SIZE);
    p.data[0] = 1;
    p.data[1] = hid;
    memcpy(&p.data[2], &payload[1], 8);
    memcpy(&p.data[10], &payload[9], 6);

    fifoPush(p, hid);   // 以 hid 去重,主/副感測器各自獨立
}

void PacketHandling::tick(HIDDevice &hidDevice) {
    if (!hidDevice.ready()) return;

    uint32_t now = millis();

    // === 第一部分:每 100ms 送一輪所有 tracker 的 register+device_info ===
    if (now - lastRegSentMs >= 100) {
        lastRegSentMs = now;

        // 高優先佇列曾經滿到必須壓縮舊狀態 → 在序列埠留下痕跡。
        // 不留的話這件事完全無聲無息,而它代表 status 的中間過程有被省略。
        if (priorityDropped != priorityDroppedLogged) {
            priorityDroppedLogged = priorityDropped;
            Serial.printf("[HID] priority queue overflow, %lu stale status packet(s) coalesced\n",
                          static_cast<unsigned long>(priorityDropped));
        }

        uint8_t report[HID_REPORT_SIZE];
        int slot = 0;
        memset(report, 0, sizeof(report));

        // 從 regRotateIndex 開始繞一圈,而不是固定從 0 開始。
        // 理由見 regRotateIndex 的宣告:send() 失敗會中斷整輪,固定從 0 開始的話
        // 清單後段的追蹤器在 USB 半通不通時永遠輪不到,在 server 上就是不存在。
        for (size_t k = 0; k < MAX_TRACKERS; k++) {
            size_t i = (regRotateIndex + k) % MAX_TRACKERS;
            if (!trackers[i].used) continue;
            if (!trackers[i].online) continue;   // 斷線的不再送 register/device_info
            TrackerInfo &ti = trackers[i];

            // 跨 task 會被改寫的欄位一次在鎖內抓成快照,底下的迴圈只用快照。
            // 這些欄位是「成對才有意義」的:sensorMask ↔ sensorSeenMs、
            // imuKnownMask ↔ imuId/magId。分開讀就可能讀到一半新一半舊的組合,
            // 而那個組合的後果不是掉一包資料,是送出型號錯誤的 device_info ——
            // server 端 Tracker.imuType 是 val,錯了就整個 session 都改不回來。
            // (寫入端 insert()/setSensorInfo() 用同一把鎖。)
            uint16_t sensorMask;
            uint16_t imuKnownMask;
            uint32_t sensorSeen[MAX_SENSORS];
            uint8_t  imuIds[MAX_SENSORS];
            uint8_t  magIds[MAX_SENSORS];
            uint8_t  defaultImu;
            uint8_t  defaultMag;
            // 寬限期的重新計時額度也要抓:額度用完之後,連「還沒看過 SensorInfo」
            // 的感測器也不能再等下去,必須直接以後備型號註冊出去(見下方 imuKnown 判斷)。
            // 少了這一項的話,額度只有主感測器(sensorId=0)受到約束:
            // registerTracker() 只在有額度時才重設 sensorSeenMs[0],但 insert()
            // 是在 sensorMask 的某個位元 0→1 時無條件重設 sensorSeenMs[s],
            // 而 freshSlot 每次都把 sensorMask 清成 1(只留主感測器)——
            // 於是重複 id 乒乓時,副感測器每次重連都拿到一份全新的寬限期,
            // 永遠等不到期、永遠不會註冊,在 server 上就是「那顆感測器不存在」。
            uint8_t  rearms;
            // 額度要連同「這一輪是不是還在進行中」一起判斷,所以 lastGraceArmMs
            // 也得抓。只看 rearms 是不夠的:沒有任何地方會隨時間把它降回去
            //(唯一的歸零點是 registerTracker() 的新一輪判斷,而那要等到下一次
            // 重新配對才會執行)。乒乓結束後那顆追蹤器就穩定連著、不再重新配對,
            // rearms 於是永遠停在用完的狀態 —— 幾分鐘後才第一次出現的副感測器
            // 會一秒都不等就以後備型號註冊,型號在 server 上永久定型。
            uint32_t lastArm;
            // id 與 mac 也一起抓:registerTracker() 在同一把鎖下改寫它們。
            // 6 byte 的 memcpy 不是原子操作,不抓快照就可能送出「半新半舊」的
            // MAC —— server 會用那個亂碼位址建立裝置,而該次 session 內裝置
            // 名稱不會再更新(沾黏)。
            uint8_t  myId;
            uint8_t  myMac[6];
            // device_info 的酬載欄位同樣要快照:freshSlot 會在鎖內把它們整批歸零,
            // 而下面是一個 byte 一個 byte 讀的 —— 中途被重新配對插進來,送出去的
            // device_info 就會是「一半舊資料、一半 0」的組合(例如電量正常但韌體
            // 版本 0.0.0)。抓快照不會讓資料變新,但至少保證它是某一個真實時刻的樣子。
            uint8_t  battSnap, battVSnap, brdSnap, mcuSnap;
            uint8_t  fwMajorSnap, fwMinorSnap, fwPatchSnap;
            uint16_t fwDateSnap;
            int8_t   rssiSnap;
            uint8_t  tempSnap[MAX_SENSORS];
            portENTER_CRITICAL(&m_mux);
            sensorMask   = ti.sensorMask;
            imuKnownMask = ti.imuKnownMask;
            memcpy(sensorSeen, ti.sensorSeenMs, sizeof(sensorSeen));
            memcpy(imuIds,     ti.imuId,        sizeof(imuIds));
            memcpy(magIds,     ti.magId,        sizeof(magIds));
            memcpy(myMac,      ti.mac,          sizeof(myMac));
            memcpy(tempSnap,   ti.temp,         sizeof(tempSnap));
            myId         = ti.id;
            defaultImu   = ti.defaultImuId;
            defaultMag   = ti.defaultMagId;
            rearms       = ti.graceRearms;
            lastArm      = ti.lastGraceArmMs;
            battSnap     = ti.batt;
            battVSnap    = ti.battV;
            brdSnap      = ti.brdId;
            mcuSnap      = ti.mcuId;
            fwDateSnap   = ti.fwDate;
            fwMajorSnap  = ti.fwMajor;
            fwMinorSnap  = ti.fwMinor;
            fwPatchSnap  = ti.fwPatch;
            rssiSnap     = ti.rssi;
            portEXIT_CRITICAL(&m_mux);

            // 對此 tracker 的每個感測器(主 + 副追蹤器)各送一組 register + device_info
            for (uint8_t s = 0; s < MAX_SENSORS; s++) {
                if (!(sensorMask & (1u << s))) continue;

                // 還沒拿到這顆感測器自己的 SensorInfo 時,先等一下再註冊。
                // server 的 Tracker.imuType 是 val:第一包 device_info 帶什麼型號
                // 就永久定型,晚到的正確型號完全沒有機會覆蓋。副感測器的
                // RotationData 又比 SensorInfo 先到,所以不等的話必然先送出 0。
                // 但等待有上限(SENSOR_INFO_GRACE_MS):寧可用主感測器的型號當後備,
                // 也不能讓一顆感測器因為 info 永遠不來而在 server 上永遠不出現。
                // 額度用完時就完全不等了:額度存在的意義就是替「等下去」設一個上限,
                // 而這個上限必須涵蓋所有感測器,不是只有主感測器(registerTracker()
                // 只保護得到 sensorSeenMs[0],insert() 對副感測器是無條件重新計時的)。
                //
                // 但「用完」必須是有時效的。乒乓一旦結束,rearms 不會自己降下來
                // (唯一的歸零點在 registerTracker(),要等下一次重新配對才跑到),
                // 所以額外要求「這一輪還在進行中」:距離上一次重新配對還沒超過
                // GRACE_EPISODE_MS。乒乓期間每次重新配對都會更新 lastGraceArmMs
                // (連額度用完的那條路徑也會更新),所以這個條件在乒乓期間恆為真;
                // 乒乓結束、安靜下來之後就自動失效,寬限期恢復正常。
                // 用有號的雙邊界:跨 core 取樣讓 lastArm 稍微「在未來」是正常的,
                // 那代表剛剛才重新配對過(這一輪絕對還在進行中);而差值大到翻號
                // (約 24.9 天沒有重新配對過)顯然不是同一輪,要判成已失效。
                int32_t sinceArm = static_cast<int32_t>(now - lastArm);
                bool episodeLive = sinceArm < static_cast<int32_t>(GRACE_EPISODE_MS)
                                && sinceArm > -static_cast<int32_t>(GRACE_EPISODE_MS);
                bool budgetSpent = (rearms > MAX_GRACE_REARMS) && episodeLive;

                bool imuKnown = (imuKnownMask & (1u << s)) != 0;
                if (!imuKnown && !budgetSpent) {
                    // 有號差值:millis() 迴繞時 (now - seen) 的無號結果仍是正確的
                    // 經過時間,只要真實間隔 < 2^31 ms(約 24.8 天)就不會誤判。
                    int32_t waited = static_cast<int32_t>(now - sensorSeen[s]);
                    // 時間戳比 now 還新(registerTracker/insert 在 lwIP task 上取
                    // millis(),與這裡的取樣差幾毫秒就可能出現)→ 當作「剛剛才第一次
                    // 看到」繼續等,而不是當作寬限期已過。
                    // 不能寫成 (waited >= 0 && waited < GRACE) 才 continue:那樣 waited
                    // 為負時會落下去用 defaultImu 註冊 —— 對主感測器而言那個值必然是 0
                    // (defaultImuId 與 imuKnownMask 同時設定),等於把 server 端的
                    // Tracker.imuType 永久寫成 UNKNOWN,而它是 val、改不回來。
                    if (waited < 0) continue;
                    if (waited < static_cast<int32_t>(SENSOR_INFO_GRACE_MS)) continue;
                }
                uint8_t imuForSensor = imuKnown ? imuIds[s] : defaultImu;
                uint8_t magForSensor = imuKnown ? magIds[s] : defaultMag;

                uint8_t hid = static_cast<uint8_t>((s << 4) | myId);

                // register (255):MAC 反序送,讓 server 顯示的硬體 ID 與直連版一致(正序),
                // 並讓 server 短名取到 MAC 唯一序號端(末三 byte)而非廠商前綴 → 不再撞名。
                // 副感測器(s>0)把最低 byte 加上 s,讓它在 server 上顯示成不同短名。
                uint8_t *r = &report[slot * HID_PACKET_SIZE];
                r[0] = 255; r[1] = hid;
                for (int b = 0; b < 6; b++) r[2 + b] = myMac[5 - b];
                if (s > 0) {
                    // 副感測器位址防撞:第一個 byte(r[7]=mac[0])整個換成合成值 0x02|(s<<4)。
                    //  - 0x02 是 locally-administered bit,ESP 工廠 MAC 一定是 0
                    //    → 副感測器位址不可能撞到任何真 MAC(主感測器)。
                    //  - sensorId 放進高 nibble → 不同 sensorId 分屬不同位址區段,
                    //    即使兩塊板子 MAC 連號、各帶多顆副感測器也不可能互撞。
                    //    (只做 |0x02 的舊做法在「連號 MAC+兩顆副感測器」時 mac[5]+s 會撞)
                    //  - r[2](=mac[5])再加 s,讓 server 短名與主感測器不同、好辨認。
                    // 全部是真 MAC 的決定性函數,重開機後穩定 → 部位記憶照樣有效。
                    r[7] = static_cast<uint8_t>(0x02 | (s << 4));
                    r[2] = static_cast<uint8_t>(r[2] + s);
                }
                slot++;
                if (slot == (int)PACKETS_PER_REPORT) {
                    // send() 失敗代表主機端沒在收(USB 未輪詢)。原本忽略回傳值繼續灌,
                    // 每次 send 都會卡在 USBHID 的 semaphore timeout(約 100ms),
                    // loop() 因此整輪停擺 → button/led/heartbeat 全部停,真正的追蹤器反而被判逾時。
                    // 送不出去就直接放棄這一輪,100ms 後再試 ——
                    // 但下一輪要從「下一顆」開始,否則後段的追蹤器永遠輪不到。
                    if (!hidDevice.send(report, HID_REPORT_SIZE)) {
                        regRotateIndex = (i + 1) % MAX_TRACKERS;
                        return;
                    }
                    memset(report, 0, sizeof(report));
                    slot = 0;
                }

                // device_info (type 0):batt/fw/rssi 整顆共用,temp/imu/mag 依 sensorId 取
                uint8_t *d = &report[slot * HID_PACKET_SIZE];
                d[0] = 0;
                d[1] = hid;
                d[2] = battSnap;
                d[3] = battVSnap;
                d[4] = tempSnap[s];
                d[5] = brdSnap;
                d[6] = mcuSnap;
                d[7] = 0;                            // resv
                d[8] = imuForSensor;
                d[9] = magForSensor;
                d[10] = fwDateSnap & 0xFF;
                d[11] = (fwDateSnap >> 8) & 0xFF;
                d[12] = fwMajorSnap;
                d[13] = fwMinorSnap;
                d[14] = fwPatchSnap;
                d[15] = static_cast<uint8_t>(rssiSnap < 0 ? -rssiSnap : rssiSnap);
                slot++;
                if (slot == (int)PACKETS_PER_REPORT) {
                    if (!hidDevice.send(report, HID_REPORT_SIZE)) {
                        regRotateIndex = (i + 1) % MAX_TRACKERS;
                        return;
                    }
                    memset(report, 0, sizeof(report));
                    slot = 0;
                }
            }
        }

        // 整輪都走完了 → 每顆都送到,下一輪回到固定順序(方便對照序列埠輸出)。
        regRotateIndex = 0;

        if (slot > 0) {
            for (int s = slot; s < (int)PACKETS_PER_REPORT; s++) {
                report[s * HID_PACKET_SIZE] = 254;
                report[s * HID_PACKET_SIZE + 1] = HID_FILLER_ID;
            }
            // 最後這一批送不出去時不需要輪替:迴圈已經走完一整圈,
            // 沒有任何追蹤器是因為提前 return 而被跳過的。
            if (!hidDevice.send(report, HID_REPORT_SIZE)) return;
        }
    }

    // === 第二部分:送高優先封包(status)+ 資料封包 ===
    static uint32_t lastDataSentMs = 0;
    if (now - lastDataSentMs < 5) {
        return;
    }
    lastDataSentMs = now;

    for (int rep = 0; rep < 8; rep++) {
        uint8_t report[HID_REPORT_SIZE];
        memset(report, 0, sizeof(report));
        int slot = 0;

        // 先填高優先封包(斷線 status 等),確保它們永不被資料壅塞延遲/丟棄。
        // 這裡只「看」不移除:send() 可能失敗(主機沒在輪詢 USB),
        // 先 pop 的話這批 status 就永遠消失,而 setTrackerOnline() 只在狀態改變時推送,
        // 不會補送 → PC 端會永遠停在錯誤狀態。確認送出成功後才 priorityDrop()。
        size_t pcount = 0;
        uint32_t ptail = 0;
        while (slot < (int)PACKETS_PER_REPORT) {
            Packet p;
            uint32_t tailNow = 0;
            if (!priorityPeek(pcount, p, tailNow)) break;
            if (pcount == 0) ptail = tailNow;
            memcpy(&report[slot * HID_PACKET_SIZE], p.data, HID_PACKET_SIZE);
            pcount++;
            slot++;
        }

        // 再用一般資料封包填滿剩餘 slot。
        // 資料封包(姿態)是連續串流,掉一兩包不影響狀態機,所以維持 pop 即可。
        while (slot < (int)PACKETS_PER_REPORT) {
            Packet p;
            if (!fifoPop(p)) break;   // fifoPop 內部有鎖;空了會回 false
            memcpy(&report[slot * HID_PACKET_SIZE], p.data, HID_PACKET_SIZE);
            slot++;
        }

        if (slot == 0) return;   // priority 和 data 都空了,結束
        for (int s = slot; s < (int)PACKETS_PER_REPORT; s++) {
            // 補位封包必須帶一個「不存在的」device id,否則 server 會把它算到
            // trackerId 0 頭上並觸發 HIDCommon 的 TIMED_OUT→OK 自動復活。
            report[s * HID_PACKET_SIZE] = 254;
            report[s * HID_PACKET_SIZE + 1] = HID_FILLER_ID;
        }

        if (!hidDevice.send(report, HID_REPORT_SIZE)) {
            return;   // 沒送出去 → 不 drop,下一輪重送同一批 status
        }
        priorityDrop(pcount, ptail);
    }
}

PacketHandling PacketHandling::instance;
