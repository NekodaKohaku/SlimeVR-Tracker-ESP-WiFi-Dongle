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
    int idx = findTracker(trackerId);
    bool isNew = (idx < 0);
    if (idx < 0) {
        for (size_t i = 0; i < MAX_TRACKERS; i++) {
            if (!trackers[i].used) { idx = static_cast<int>(i); break; }
        }
    }
    if (idx < 0) return false;
    trackers[idx].used = true;
    trackers[idx].id = trackerId;
    memcpy(trackers[idx].mac, mac, 6);
    if (isNew) trackers[idx].sensorMask = 1;   // 新註冊:先只有主感測器,副追蹤器等資料進來再補
    return isNew;
}

void PacketHandling::updateRssiByMac(const uint8_t mac[6], int8_t rssi) {
    for (size_t i = 0; i < MAX_TRACKERS; i++)
        if (trackers[i].used && memcmp(trackers[i].mac, mac, 6) == 0) {
            trackers[i].rssi = rssi;
            return;
        }
}

// ---- 官方相容 setter:拿到就更新對應欄位 ----
void PacketHandling::setBattery(uint8_t trackerId, uint8_t pct, uint16_t mv) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    trackers[idx].batt = pct;
    trackers[idx].battV = static_cast<uint8_t>((mv > 2450) ? (mv - 2450) / 10 : 0);
    trackers[idx].hasBattData = true;
}

void PacketHandling::setTemp(uint8_t trackerId, uint8_t tempEncoded) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    trackers[idx].temp = tempEncoded;
}

void PacketHandling::setRssi(uint8_t trackerId, int8_t rssi) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    trackers[idx].rssi = rssi;
}

void PacketHandling::setSensorInfo(uint8_t trackerId, uint8_t imuId, uint8_t magId) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    trackers[idx].imuId = imuId;
    trackers[idx].magId = magId;
}

void PacketHandling::setFirmware(uint8_t trackerId, uint8_t brdId, uint8_t mcuId,
                                 uint16_t fwDate, uint8_t fwMajor, uint8_t fwMinor, uint8_t fwPatch) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    trackers[idx].brdId = brdId;
    trackers[idx].mcuId = mcuId;
    trackers[idx].fwDate = fwDate;
    trackers[idx].fwMajor = fwMajor;
    trackers[idx].fwMinor = fwMinor;
    trackers[idx].fwPatch = fwPatch;
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

    if (trackers[idx].online == online) return;   // 狀態沒變,不重複處理
    trackers[idx].online = online;

    // 對這顆 tracker 的每個感測器(主+副)都送狀態封包。
    // 離線時送 TIMED_OUT(5) 而非 DISCONNECTED(0):server 只會在 TIMED_OUT 狀態下
    // 讓「進來的資料」自動把狀態救回 OK(HIDCommon.kt),DISCONNECTED 則要等明確的
    // status=OK 才會恢復。用 TIMED_OUT 可避免「顯示離線、資料卻一直進來」卡住。
    for (uint8_t s = 0; s < MAX_SENSORS; s++) {
        if (!(trackers[idx].sensorMask & (1u << s))) continue;
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
    // 去重:同一感測器(hidId=(sensorId<<4)|trackerId)的資料封包(data[0]==1)已在佇列就原地更新
    if (!fifoEmpty()) {
        size_t idx = fifoTail;
        while (idx != fifoHead) {
            if (fifo[idx].data[1] == hidId && fifo[idx].data[0] == 1) {
                fifo[idx] = p;
                portEXIT_CRITICAL(&m_mux);
                return;
            }
            idx = (idx + 1) % FIFO_SIZE;
        }
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
        portEXIT_CRITICAL(&m_mux);
        return;   // 滿了就丟(極少發生;16 格對 status 綽綽有餘)
    }
    priorityFifo[priorityHead] = p;
    priorityHead = (priorityHead + 1) % PRIORITY_FIFO_SIZE;
    if (priorityHead == priorityTail) priorityFull = true;
    portEXIT_CRITICAL(&m_mux);
}

bool PacketHandling::priorityPop(Packet &out) {
    portENTER_CRITICAL(&m_mux);
    if (priorityEmpty()) {
        portEXIT_CRITICAL(&m_mux);
        return false;
    }
    out = priorityFifo[priorityTail];
    priorityTail = (priorityTail + 1) % PRIORITY_FIFO_SIZE;
    priorityFull = false;
    portEXIT_CRITICAL(&m_mux);
    return true;
}

void PacketHandling::insert(const uint8_t *payload) {
    uint8_t trackerId = payload[0] >> 4;
    uint8_t sensorId  = payload[0] & 0x0F;   // 副追蹤器:主感測器=0,副感測器=1..

    int idx = findTracker(trackerId);
    if (idx < 0) return;

    // 第一次看到這個 sensorId → 記錄下來,並立刻補送 register/device_info,
    // 讓 server 把它註冊成一顆獨立的 tracker(否則資料會被 server 丟棄)。
    if (!(trackers[idx].sensorMask & (1u << sensorId))) {
        trackers[idx].sensorMask |= (1u << sensorId);
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

void PacketHandling::insertInfo(const uint8_t *info) {
    uint8_t trackerId = info[0];
    int idx = findTracker(trackerId);
    if (idx < 0) return;

    trackers[idx].batt = info[1];
    uint16_t mv = info[2] | (info[3] << 8);
    trackers[idx].battV = static_cast<uint8_t>((mv > 2450) ? (mv - 2450) / 10 : 0);
    trackers[idx].temp = info[4];
    trackers[idx].brdId = info[5];
    trackers[idx].mcuId = info[6];
    trackers[idx].imuId = info[7];
    trackers[idx].magId = info[8];
    trackers[idx].fwDate = info[9] | (info[10] << 8);
    trackers[idx].fwMajor = info[11];
    trackers[idx].fwMinor = info[12];
    trackers[idx].fwPatch = info[13];
    trackers[idx].hasBattData = true;
}

void PacketHandling::tick(HIDDevice &hidDevice) {
    if (!hidDevice.ready()) return;

    uint32_t now = millis();

    // === 第一部分:每 100ms 送一輪所有 tracker 的 register+device_info ===
    if (now - lastRegSentMs >= 100) {
        lastRegSentMs = now;

        uint8_t report[HID_REPORT_SIZE];
        int slot = 0;
        memset(report, 0, sizeof(report));

        for (size_t i = 0; i < MAX_TRACKERS; i++) {
            if (!trackers[i].used) continue;
            if (!trackers[i].online) continue;   // 斷線的不再送 register/device_info
            TrackerInfo &ti = trackers[i];

            // 對此 tracker 的每個感測器(主 + 副追蹤器)各送一組 register + device_info
            for (uint8_t s = 0; s < MAX_SENSORS; s++) {
                if (!(ti.sensorMask & (1u << s))) continue;
                uint8_t hid = static_cast<uint8_t>((s << 4) | ti.id);

                // register (255):MAC 反序送,讓 server 顯示的硬體 ID 與直連版一致(正序),
                // 並讓 server 短名取到 MAC 唯一序號端(末三 byte)而非廠商前綴 → 不再撞名。
                // 副感測器(s>0)把最低 byte 加上 s,讓它在 server 上顯示成不同短名。
                uint8_t *r = &report[slot * HID_PACKET_SIZE];
                r[0] = 255; r[1] = hid;
                for (int b = 0; b < 6; b++) r[2 + b] = ti.mac[5 - b];
                if (s > 0) {
                    // 副感測器位址防撞:ESP 工廠 MAC 的 mac[0](=這裡的 r[7])locally-administered
                    // bit(0x02)一定是 0。這裡只把這個 bit 設成 1 → 副感測器位址「數學上」不可能
                    // 等於任何主感測器的真 MAC(所以絕不會撞名/搶部位設定),其餘 MAC byte 保留原值,
                    // server 顯示的位址仍看得出是同一顆的家族。再把最低 byte(r[2]=mac[5])加上 s,
                    // 用來區分同一顆的多個副感測器、也讓短名和主感測器不同。
                    // 這兩步都是真 MAC 的決定性函數,重開機後仍穩定 → 部位記憶照樣有效。
                    r[7] = static_cast<uint8_t>(r[7] | 0x02);
                    r[2] = static_cast<uint8_t>(r[2] + s);
                }
                slot++;
                if (slot == (int)PACKETS_PER_REPORT) {
                    hidDevice.send(report, HID_REPORT_SIZE);
                    memset(report, 0, sizeof(report));
                    slot = 0;
                }

                // device_info (type 0):batt/temp/fw/rssi 為整顆 tracker 共用
                uint8_t *d = &report[slot * HID_PACKET_SIZE];
                d[0] = 0;
                d[1] = hid;
                d[2] = ti.batt;
                d[3] = ti.battV;
                d[4] = ti.temp;
                d[5] = ti.brdId;
                d[6] = ti.mcuId;
                d[7] = 0;                            // resv
                d[8] = ti.imuId;
                d[9] = ti.magId;
                d[10] = ti.fwDate & 0xFF;
                d[11] = (ti.fwDate >> 8) & 0xFF;
                d[12] = ti.fwMajor;
                d[13] = ti.fwMinor;
                d[14] = ti.fwPatch;
                d[15] = static_cast<uint8_t>(ti.rssi < 0 ? -ti.rssi : ti.rssi);
                slot++;
                if (slot == (int)PACKETS_PER_REPORT) {
                    hidDevice.send(report, HID_REPORT_SIZE);
                    memset(report, 0, sizeof(report));
                    slot = 0;
                }
            }
        }

        if (slot > 0) {
            for (int s = slot; s < (int)PACKETS_PER_REPORT; s++) {
                report[s * HID_PACKET_SIZE] = 254;
            }
            hidDevice.send(report, HID_REPORT_SIZE);
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

        // 先填高優先封包(斷線 status 等),確保它們永不被資料壅塞延遲/丟棄
        while (slot < (int)PACKETS_PER_REPORT) {
            Packet p;
            if (!priorityPop(p)) break;
            memcpy(&report[slot * HID_PACKET_SIZE], p.data, HID_PACKET_SIZE);
            slot++;
        }

        // 再用一般資料封包填滿剩餘 slot
        while (slot < (int)PACKETS_PER_REPORT) {
            Packet p;
            if (!fifoPop(p)) break;   // fifoPop 內部有鎖;空了會回 false
            memcpy(&report[slot * HID_PACKET_SIZE], p.data, HID_PACKET_SIZE);
            slot++;
        }

        if (slot == 0) return;   // priority 和 data 都空了,結束
        for (int s = slot; s < (int)PACKETS_PER_REPORT; s++) {
            report[s * HID_PACKET_SIZE] = 254;
        }

        if (!hidDevice.send(report, HID_REPORT_SIZE)) {
            return;
        }
    }
}

PacketHandling PacketHandling::instance;
