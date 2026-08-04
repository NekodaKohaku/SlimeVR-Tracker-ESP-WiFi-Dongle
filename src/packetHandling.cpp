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

    trackers[idx].id = trackerId;
    memcpy(trackers[idx].mac, mac, 6);
    if (isNew) trackers[idx].sensorMask = 1;
    trackers[idx].used = true;
    return isNew;
}

void PacketHandling::updateRssiByMac(const uint8_t mac[6], int8_t rssi) {
    for (size_t i = 0; i < MAX_TRACKERS; i++)
        if (trackers[i].used && memcmp(trackers[i].mac, mac, 6) == 0) {
            trackers[i].rssi = rssi;
            return;
        }
}

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

    portENTER_CRITICAL(&m_mux);
    bool changed = (trackers[idx].online != online);
    if (changed) trackers[idx].online = online;
    portEXIT_CRITICAL(&m_mux);
    if (!changed) return;

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
        droppedPackets++;
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
        droppedPackets++;
        portEXIT_CRITICAL(&m_mux);
        return;
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

PacketHandling::Stats PacketHandling::getStats() {
    portENTER_CRITICAL(&m_mux);
    Stats result{droppedPackets, failedHidReports};
    portEXIT_CRITICAL(&m_mux);
    return result;
}

void PacketHandling::recordHidFailure(size_t lostPackets) {
    portENTER_CRITICAL(&m_mux);
    failedHidReports++;
    droppedPackets += lostPackets;
    portEXIT_CRITICAL(&m_mux);
}

void PacketHandling::insert(const uint8_t *payload) {
    uint8_t trackerId = payload[0] >> 4;
    uint8_t sensorId  = payload[0] & 0x0F;

    int idx = findTracker(trackerId);
    if (idx < 0) return;

    if (!(trackers[idx].sensorMask & (1u << sensorId))) {
        trackers[idx].sensorMask |= (1u << sensorId);
        lastRegSentMs = 0;
    }

    uint8_t hid = static_cast<uint8_t>((sensorId << 4) | trackerId);

    Packet p;
    memset(p.data, 0, HID_PACKET_SIZE);
    p.data[0] = 1;
    p.data[1] = hid;
    memcpy(&p.data[2], &payload[1], 8);
    memcpy(&p.data[10], &payload[9], 6);

    fifoPush(p, hid);
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

    if (now - lastRegSentMs >= 100) {
        lastRegSentMs = now;

        uint8_t report[HID_REPORT_SIZE];
        int slot = 0;
        memset(report, 0, sizeof(report));

        for (size_t i = 0; i < MAX_TRACKERS; i++) {
            if (!trackers[i].used) continue;
            if (!trackers[i].online) continue;
            TrackerInfo &ti = trackers[i];

            for (uint8_t s = 0; s < MAX_SENSORS; s++) {
                if (!(ti.sensorMask & (1u << s))) continue;
                uint8_t hid = static_cast<uint8_t>((s << 4) | ti.id);

                uint8_t *r = &report[slot * HID_PACKET_SIZE];
                r[0] = 255; r[1] = hid;
                for (int b = 0; b < 6; b++) r[2 + b] = ti.mac[5 - b];
                if (s > 0) {

                    r[7] = static_cast<uint8_t>(0x02 | (s << 4));
                    r[2] = static_cast<uint8_t>(r[2] + s);
                }
                slot++;
                if (slot == (int)PACKETS_PER_REPORT) {
                    if (!hidDevice.send(report, HID_REPORT_SIZE)) {
                        recordHidFailure(0);
                    }
                    memset(report, 0, sizeof(report));
                    slot = 0;
                }

                uint8_t *d = &report[slot * HID_PACKET_SIZE];
                d[0] = 0;
                d[1] = hid;
                d[2] = ti.batt;
                d[3] = ti.battV;
                d[4] = ti.temp;
                d[5] = ti.brdId;
                d[6] = ti.mcuId;
                d[7] = 0;
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
                    if (!hidDevice.send(report, HID_REPORT_SIZE)) {
                        recordHidFailure(0);
                    }
                    memset(report, 0, sizeof(report));
                    slot = 0;
                }
            }
        }

        if (slot > 0) {
            for (int s = slot; s < (int)PACKETS_PER_REPORT; s++) {
                report[s * HID_PACKET_SIZE] = 254;
            }
            if (!hidDevice.send(report, HID_REPORT_SIZE)) {
                recordHidFailure(0);
            }
        }
    }

    static uint32_t lastDataSentMs = 0;
    if (now - lastDataSentMs < 5) {
        return;
    }
    lastDataSentMs = now;

    for (int rep = 0; rep < 8; rep++) {
        uint8_t report[HID_REPORT_SIZE];
        memset(report, 0, sizeof(report));
        int slot = 0;

        while (slot < (int)PACKETS_PER_REPORT) {
            Packet p;
            if (!priorityPop(p)) break;
            memcpy(&report[slot * HID_PACKET_SIZE], p.data, HID_PACKET_SIZE);
            slot++;
        }

        while (slot < (int)PACKETS_PER_REPORT) {
            Packet p;
            if (!fifoPop(p)) break;
            memcpy(&report[slot * HID_PACKET_SIZE], p.data, HID_PACKET_SIZE);
            slot++;
        }

        if (slot == 0) return;
        for (int s = slot; s < (int)PACKETS_PER_REPORT; s++) {
            report[s * HID_PACKET_SIZE] = 254;
        }

        if (!hidDevice.send(report, HID_REPORT_SIZE)) {
            recordHidFailure(static_cast<size_t>(slot));
            return;
        }
    }
}

PacketHandling PacketHandling::instance;
