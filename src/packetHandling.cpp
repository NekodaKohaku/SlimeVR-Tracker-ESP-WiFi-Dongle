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

    bool freshSlot = isNew || memcmp(trackers[idx].mac, mac, 6) != 0;

    uint32_t nowMs = millis();
    bool budgetExhausted = false;

    portENTER_CRITICAL(&m_mux);
    trackers[idx].id = trackerId;
    memcpy(trackers[idx].mac, mac, 6);
    if (freshSlot) {

        int32_t sinceLastArm = static_cast<int32_t>(nowMs - trackers[idx].lastGraceArmMs);
        if (sinceLastArm < 0 || sinceLastArm >= static_cast<int32_t>(GRACE_EPISODE_MS)) {
            trackers[idx].graceRearms = 0;
        }
        trackers[idx].lastGraceArmMs = nowMs;
        trackers[idx].online = true;
        trackers[idx].sensorMask = 1;
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

        if (trackers[idx].graceRearms < MAX_GRACE_REARMS) {

            trackers[idx].graceRearms++;
            trackers[idx].sensorSeenMs[0] = nowMs;
        } else if (trackers[idx].graceRearms == MAX_GRACE_REARMS) {

            trackers[idx].graceRearms++;
            budgetExhausted = true;
        }

    }
    trackers[idx].used = true;
    portEXIT_CRITICAL(&m_mux);

    if (budgetExhausted) {
        Serial.printf("[HID] tracker %u re-paired %u times in a row (< %lu ms apart); no longer waiting "
                      "for sensor info, will register with unknown IMU type (two trackers sharing an id?)\n",
                      static_cast<unsigned>(trackerId),
                      static_cast<unsigned>(MAX_GRACE_REARMS) + 1u,
                      static_cast<unsigned long>(GRACE_EPISODE_MS));
    }
    return isNew;
}

void PacketHandling::setBattery(uint8_t trackerId, uint8_t pct, uint16_t mv) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;

    uint16_t enc = (mv > 2450) ? static_cast<uint16_t>((mv - 2450) / 10) : 0;
    uint8_t encV = static_cast<uint8_t>(enc > 255 ? 255 : enc);

    portENTER_CRITICAL(&m_mux);
    trackers[idx].batt = pct;
    trackers[idx].battV = encV;
    trackers[idx].hasBattData = true;
    portEXIT_CRITICAL(&m_mux);
}

void PacketHandling::setTemp(uint8_t trackerId, uint8_t sensorId, uint8_t tempEncoded) {
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    trackers[idx].temp[sensorId & 0x0F] = tempEncoded;
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

    portENTER_CRITICAL(&m_mux);
    trackers[idx].imuId[s] = imuId;
    trackers[idx].magId[s] = magId;

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
    uint16_t sensorMask = trackers[idx].sensorMask;
    portEXIT_CRITICAL(&m_mux);
    if (!changed) return;

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

        priorityTail = (priorityTail + 1) % PRIORITY_FIFO_SIZE;
        priorityTailSeq++;
        priorityFull = false;
        priorityDropped++;
    }
    priorityFifo[priorityHead] = p;
    priorityHead = (priorityHead + 1) % PRIORITY_FIFO_SIZE;
    if (priorityHead == priorityTail) priorityFull = true;
    portEXIT_CRITICAL(&m_mux);
}

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
    uint8_t sensorId  = payload[0] & 0x0F;

    int idx = findTracker(trackerId);
    if (idx < 0) return;

    if (!(trackers[idx].sensorMask & (1u << sensorId))) {

        uint32_t seenMs = millis();
        portENTER_CRITICAL(&m_mux);
        trackers[idx].sensorSeenMs[sensorId] = seenMs;
        trackers[idx].sensorMask |= (1u << sensorId);
        portEXIT_CRITICAL(&m_mux);
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

void PacketHandling::tick(HIDDevice &hidDevice) {
    if (!hidDevice.ready()) return;

    uint32_t now = millis();

    if (now - lastRegSentMs >= 100) {
        lastRegSentMs = now;

        if (priorityDropped != priorityDroppedLogged) {
            priorityDroppedLogged = priorityDropped;
            Serial.printf("[HID] priority queue overflow, %lu stale status packet(s) coalesced\n",
                          static_cast<unsigned long>(priorityDropped));
        }

        uint8_t report[HID_REPORT_SIZE];
        int slot = 0;
        memset(report, 0, sizeof(report));

        for (size_t k = 0; k < MAX_TRACKERS; k++) {
            size_t i = (regRotateIndex + k) % MAX_TRACKERS;
            if (!trackers[i].used) continue;
            if (!trackers[i].online) continue;
            TrackerInfo &ti = trackers[i];

            uint16_t sensorMask;
            uint16_t imuKnownMask;
            uint32_t sensorSeen[MAX_SENSORS];
            uint8_t  imuIds[MAX_SENSORS];
            uint8_t  magIds[MAX_SENSORS];
            uint8_t  defaultImu;
            uint8_t  defaultMag;

            uint8_t  rearms;

            uint32_t lastArm;

            uint8_t  myId;
            uint8_t  myMac[6];

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

            for (uint8_t s = 0; s < MAX_SENSORS; s++) {
                if (!(sensorMask & (1u << s))) continue;

                int32_t sinceArm = static_cast<int32_t>(now - lastArm);
                bool episodeLive = sinceArm < static_cast<int32_t>(GRACE_EPISODE_MS)
                                && sinceArm > -static_cast<int32_t>(GRACE_EPISODE_MS);
                bool budgetSpent = (rearms > MAX_GRACE_REARMS) && episodeLive;

                bool imuKnown = (imuKnownMask & (1u << s)) != 0;
                if (!imuKnown && !budgetSpent) {

                    int32_t waited = static_cast<int32_t>(now - sensorSeen[s]);

                    if (waited < 0) continue;
                    if (waited < static_cast<int32_t>(SENSOR_INFO_GRACE_MS)) continue;
                }
                uint8_t imuForSensor = imuKnown ? imuIds[s] : defaultImu;
                uint8_t magForSensor = imuKnown ? magIds[s] : defaultMag;

                uint8_t hid = static_cast<uint8_t>((s << 4) | myId);

                uint8_t *r = &report[slot * HID_PACKET_SIZE];
                r[0] = 255; r[1] = hid;
                for (int b = 0; b < 6; b++) r[2 + b] = myMac[5 - b];
                if (s > 0) {

                    r[7] = static_cast<uint8_t>(0x02 | (s << 4));
                    r[2] = static_cast<uint8_t>(r[2] + s);
                }
                slot++;
                if (slot == (int)PACKETS_PER_REPORT) {

                    if (!hidDevice.send(report, HID_REPORT_SIZE)) {
                        regRotateIndex = (i + 1) % MAX_TRACKERS;
                        return;
                    }
                    memset(report, 0, sizeof(report));
                    slot = 0;
                }

                uint8_t *d = &report[slot * HID_PACKET_SIZE];
                d[0] = 0;
                d[1] = hid;
                d[2] = battSnap;
                d[3] = battVSnap;
                d[4] = tempSnap[s];
                d[5] = brdSnap;
                d[6] = mcuSnap;
                d[7] = 0;
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

        regRotateIndex = 0;

        if (slot > 0) {
            for (int s = slot; s < (int)PACKETS_PER_REPORT; s++) {
                report[s * HID_PACKET_SIZE] = 254;
                report[s * HID_PACKET_SIZE + 1] = HID_FILLER_ID;
            }

            if (!hidDevice.send(report, HID_REPORT_SIZE)) return;
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

        while (slot < (int)PACKETS_PER_REPORT) {
            Packet p;
            if (!fifoPop(p)) break;
            memcpy(&report[slot * HID_PACKET_SIZE], p.data, HID_PACKET_SIZE);
            slot++;
        }

        if (slot == 0) return;
        for (int s = slot; s < (int)PACKETS_PER_REPORT; s++) {

            report[s * HID_PACKET_SIZE] = 254;
            report[s * HID_PACKET_SIZE + 1] = HID_FILLER_ID;
        }

        if (!hidDevice.send(report, HID_REPORT_SIZE)) {
            return;
        }
        priorityDrop(pcount, ptail);
    }
}

PacketHandling PacketHandling::instance;
