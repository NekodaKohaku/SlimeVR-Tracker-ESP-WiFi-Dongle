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

void PacketHandling::pushStatus(uint8_t trackerId, uint8_t status) {
    Packet p;
    memset(p.data, 0, HID_PACKET_SIZE);
    p.data[0] = 3;
    p.data[1] = trackerId;
    p.data[2] = status;
    priorityPush(p);
}

void PacketHandling::setTrackerOnline(uint8_t trackerId, bool online) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;

    if (trackers[idx].online == online) return;   // 狀態沒變,不重複處理
    trackers[idx].online = online;

    if (!online) {
        pushStatus(trackerId, 0);
        Serial.printf("[HID] tracker %u marked DISCONNECTED\n", trackerId);
    } else {
        pushStatus(trackerId, 1);
        lastRegSentMs = 0;
        Serial.printf("[HID] tracker %u back ONLINE\n", trackerId);
    }
}

void PacketHandling::fifoPush(const Packet &p, uint8_t trackerId) {
    portENTER_CRITICAL(&m_mux);
    // 去重:同一 tracker 的資料封包(data[0]==1)已在佇列就原地更新
    if (!fifoEmpty()) {
        size_t idx = fifoTail;
        while (idx != fifoHead) {
            if (fifo[idx].data[1] == trackerId && fifo[idx].data[0] == 1) {
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

    int idx = findTracker(trackerId);
    if (idx < 0) return;

    Packet p;
    memset(p.data, 0, HID_PACKET_SIZE);
    p.data[0] = 1;
    p.data[1] = trackerId;
    memcpy(&p.data[2], &payload[1], 8);
    memcpy(&p.data[10], &payload[9], 6);

    fifoPush(p, trackerId);
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

            // register (255)
            uint8_t *r = &report[slot * HID_PACKET_SIZE];
            // register (255):MAC 反序送,讓 server 顯示的硬體 ID 與直連版一致(正序),
            // 並讓 server 短名取到 MAC 唯一序號端(末三 byte)而非廠商前綴 → 不再撞名
            r[0] = 255; r[1] = ti.id;
            for (int b = 0; b < 6; b++) r[2 + b] = ti.mac[5 - b];
            slot++;
            if (slot == (int)PACKETS_PER_REPORT) {
                hidDevice.send(report, HID_REPORT_SIZE);
                memset(report, 0, sizeof(report));
                slot = 0;
            }

            // device_info (type 0)
            uint8_t *d = &report[slot * HID_PACKET_SIZE];
            d[0] = 0;
            d[1] = ti.id;
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
