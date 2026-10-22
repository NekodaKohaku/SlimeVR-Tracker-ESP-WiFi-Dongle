#include "packetHandling.h"

namespace {
struct StateLock {
    portMUX_TYPE &mux;
    explicit StateLock(portMUX_TYPE &m) : mux(m) { portENTER_CRITICAL(&mux); }
    ~StateLock() { portEXIT_CRITICAL(&mux); }
};
}


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
    if (trackerId >= MAX_TRACKERS) return false;
    StateLock lock(m_mux);
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
    StateLock lock(m_mux);
    for (size_t i = 0; i < MAX_TRACKERS; i++)
        if (trackers[i].used && memcmp(trackers[i].mac, mac, 6) == 0) {
            trackers[i].rssi = rssi;
            return;
        }
}

void PacketHandling::setBattery(uint8_t trackerId, uint8_t pct, uint16_t mv) {
    StateLock lock(m_mux);
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    trackers[idx].batt = pct;
    trackers[idx].battV = static_cast<uint8_t>((mv > 2450) ? (mv - 2450) / 10 : 0);
    trackers[idx].hasBattData = true;
}

void PacketHandling::setTemp(uint8_t trackerId, uint8_t tempEncoded, uint8_t sensorId) {
    StateLock lock(m_mux);
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    if (sensorId >= MAX_SENSORS) return;
    auto &sensor = sensors[(sensorId << 4) | trackerId];
    sensor.temp = tempEncoded; sensor.hasTemperature = true;
    if (sensorId == 0) trackers[idx].temp = tempEncoded;
}

void PacketHandling::setRssi(uint8_t trackerId, int8_t rssi) {
    StateLock lock(m_mux);
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    trackers[idx].rssi = rssi;
}

void PacketHandling::setSensorInfo(uint8_t trackerId, uint8_t imuId, uint8_t magId, uint8_t sensorId, int sensorState) {
    StateLock lock(m_mux);
    int idx = findTracker(trackerId);
    if (idx < 0) return;
    if (sensorId >= MAX_SENSORS) return;
    auto &sensor = sensors[(sensorId << 4) | trackerId];
    sensor.imuId = imuId; sensor.magId = magId; sensor.hasInfo = true;
    sensor.sensorState = sensorState;
    if (sensorId == 0) { trackers[idx].imuId = imuId; trackers[idx].magId = magId; }
}

void PacketHandling::setFirmware(uint8_t trackerId, uint8_t brdId, uint8_t mcuId,
                                 uint16_t fwDate, uint8_t fwMajor, uint8_t fwMinor, uint8_t fwPatch) {
    StateLock lock(m_mux);
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

    portENTER_CRITICAL(&m_mux);
    int idx = findTracker(static_cast<uint8_t>(hidId & 0x0F));
    if (idx >= 0) {
        int8_t r = trackers[idx].rssi;
        p.data[15] = static_cast<uint8_t>(r < 0 ? -r : r);
    }
    portEXIT_CRITICAL(&m_mux);
    priorityPush(p);
}

void PacketHandling::setTrackerOnline(uint8_t trackerId, bool online) {
    portENTER_CRITICAL(&m_mux);
    int idx = findTracker(trackerId);
    if (idx < 0) { portEXIT_CRITICAL(&m_mux); return; }
    bool changed = (trackers[idx].online != online);
    if (changed) trackers[idx].online = online;
    uint16_t mask = trackers[idx].sensorMask;
    if (changed) {
        for (uint8_t sensor = 0; sensor < MAX_SENSORS; sensor++) {
            uint8_t hid = static_cast<uint8_t>((sensor << 4) | trackerId);
            if (online) registeredHid[hid] = false;
            else poseDirty[hid] = false;
        }
    }
    portEXIT_CRITICAL(&m_mux);
    if (!changed) return;

    for (uint8_t s = 0; s < MAX_SENSORS; s++) {
        if (!(mask & (1u << s))) continue;
        uint8_t hid = static_cast<uint8_t>((s << 4) | trackerId);
        pushStatusHid(hid, online ? 1 : 5);
    }

    if (online) {
        Serial.printf("[HID] tracker %u back ONLINE\n", trackerId);
    } else {
        Serial.printf("[HID] tracker %u marked TIMED_OUT\n", trackerId);
    }
}

void PacketHandling::priorityPush(const Packet &p) {
    portENTER_CRITICAL(&m_mux);
    if (priorityFull) {
        droppedPackets++;
        int idx = findTracker(p.data[1] & 15);
        if (idx >= 0) trackers[idx].controlDrops++;
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
    Stats result{droppedPackets, failedHidReports, coalescedPoses, successfulHidReports, rejectedUdpPackets};
    portEXIT_CRITICAL(&m_mux);
    return result;
}

void PacketHandling::insert(const uint8_t *payload) {
    StateLock lock(m_mux);
    uint8_t trackerId = payload[0] >> 4;
    uint8_t sensorId  = payload[0] & 0x0F;

    int idx = findTracker(trackerId);
    if (idx < 0) return;

    if (!(trackers[idx].sensorMask & (1u << sensorId))) {
        trackers[idx].sensorMask |= (1u << sensorId);
    }

    uint8_t hid = static_cast<uint8_t>((sensorId << 4) | trackerId);

    Packet p;
    memset(p.data, 0, HID_PACKET_SIZE);
    p.data[0] = 1;
    p.data[1] = hid;
    memcpy(&p.data[2], &payload[1], 8);
    memcpy(&p.data[10], &payload[9], 6);

    if (poseDirty[hid]) { coalescedPoses++; trackers[idx].coalesced++; }
    sensors[hid].hasPose = true;
    sensors[hid].lastPoseMs = millis();
    sensors[hid].poses++;
    latestPose[hid] = p;
    poseVersion[hid]++;
    poseDirty[hid] = true;
}

void PacketHandling::insertInfo(const uint8_t *info) {
    StateLock lock(m_mux);
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

void PacketHandling::forgetTracker(uint8_t trackerId) {
    if (trackerId >= MAX_TRACKERS) return;
    uint16_t mask = 0;
    {
        StateLock lock(m_mux);
        int idx = findTracker(trackerId);
        if (idx >= 0) { mask = trackers[idx].sensorMask; trackers[idx] = TrackerInfo{}; }
        for (unsigned sensor = 0; sensor < MAX_SENSORS; sensor++) {
            uint8_t hid = static_cast<uint8_t>((sensor << 4) | trackerId);
            poseDirty[hid] = false; registeredHid[hid] = false; sensors[hid] = SensorInfo{};
        }
        // Remove stale records for this identity, preserving the others' order.
        Packet kept[PRIORITY_FIFO_SIZE]; size_t count = 0;
        while (!priorityEmpty()) {
            Packet record = priorityFifo[priorityTail];
            priorityTail = (priorityTail + 1) % PRIORITY_FIFO_SIZE; priorityFull = false;
            if ((record.data[1] & 15) != trackerId) kept[count++] = record;
        }
        priorityHead = count % PRIORITY_FIFO_SIZE; priorityTail = 0;
        priorityFull = count == PRIORITY_FIFO_SIZE;
        for (size_t i = 0; i < count; i++) priorityFifo[i] = kept[i];
        if (pending) for (size_t i = 0; i < PACKETS_PER_REPORT; i++) {
            uint8_t *record = pendingReport + i * HID_PACKET_SIZE;
            if (record[0] != 254 && (record[1] & 15) == trackerId) record[0] = 254;
        }
    }
    for (uint8_t sensor = 0; sensor < MAX_SENSORS; sensor++)
        if (mask & (1u << sensor)) pushStatusHid((sensor << 4) | trackerId, 0);
}

void PacketHandling::printSensors(Print &out, uint8_t trackerId) {
    if (trackerId >= MAX_TRACKERS) return;
    SensorInfo snapshot[MAX_SENSORS]; uint16_t mask = 0; bool online = false;
    {
        StateLock lock(m_mux);
        int idx = findTracker(trackerId);
        if (idx >= 0) { mask = trackers[idx].sensorMask; online = trackers[idx].online; }
        for (unsigned sensor = 0; sensor < MAX_SENSORS; sensor++) snapshot[sensor] = sensors[(sensor << 4) | trackerId];
    }
    uint32_t now = millis();
    out.println("Sensors v1: tracker,sensor,recent_pose,age_ms,reported_state,imu_id,temp_encoded,poses");
    for (uint8_t sensor = 0; sensor < MAX_SENSORS; sensor++) {
        const auto &s = snapshot[sensor];
        if (!(mask & (1u << sensor)) && !s.hasInfo && !s.hasTemperature) continue;
        uint32_t age = now - s.lastPoseMs;
        out.printf("S,%u,%u,%u,%ld,%d,%d,%d,%u\n", trackerId, sensor,
                   online && s.hasPose && age < 1500 ? 1 : 0,
                   s.hasPose ? static_cast<long>(age) : -1L,
                   s.sensorState, s.hasInfo ? s.imuId : -1,
                   s.hasTemperature ? s.temp : -1, s.poses);
    }
    out.println("Sensors end");
}

void PacketHandling::recordRejectedUdp(uint8_t trackerId) {
    StateLock lock(m_mux);
    rejectedUdpPackets++;
    int idx = findTracker(trackerId);
    if (idx >= 0) trackers[idx].rejectedUdp++;
}

void PacketHandling::recordReception(uint8_t trackerId) {
    uint32_t now = millis();
    portENTER_CRITICAL(&m_mux);
    int idx = findTracker(trackerId);
    if (idx >= 0) {
        auto &t = trackers[idx];
        if (t.received == 0) t.rateStartMs = now;
        t.received++;
        t.lastRxMs = now;
        t.rateCount++;
        uint32_t elapsed = now - t.rateStartMs;
        if (elapsed >= 1000) {
            t.pps = static_cast<uint32_t>((uint64_t(t.rateCount) * 1000) / elapsed);
            t.rateCount = 0;
            t.rateStartMs = now;
        }
    }
    portEXIT_CRITICAL(&m_mux);
}

void PacketHandling::printDiagnostics(Print &out) {
    // Snapshot under the lock; format/write USB console outside it.
    TrackerInfo snapshot[MAX_TRACKERS];
    portENTER_CRITICAL(&m_mux);
    memcpy(snapshot, trackers, sizeof(snapshot));
    portEXIT_CRITICAL(&m_mux);
    uint32_t now = millis();
    out.println("Diagnostics v1: id,online,rssi,udp_pps,age_ms,sensor_mask,rx,coalesced,usb_failed_attempts,rejected_udp,control_drops");
    for (const auto &t : snapshot) {
        if (!t.used) continue;
        uint32_t age = now - t.lastRxMs;
        out.printf("D,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%u\n", t.id, t.online ? 1 : 0,
                   t.rssi, age >= 1500 ? 0 : t.pps, age, t.sensorMask,
                   t.received, t.coalesced, t.usbFailures, t.rejectedUdp, t.controlDrops);
    }
    out.println("Diagnostics end");
}

void PacketHandling::tick(HIDDevice &hidDevice) {
    if (!hidDevice.ready()) return;
    uint32_t now = millis();
    if (now - lastSubmitMs < 1) return;
    lastSubmitMs = now;

    if (!pending) {
        memset(pendingReport, 0, sizeof(pendingReport));
        for (size_t i = 0; i < PACKETS_PER_REPORT; i++) pendingReport[i * HID_PACKET_SIZE] = 254;
        memset(pendingVersion, 0, sizeof(pendingVersion));
        size_t slot = 0;
        // At most one registration/info pair per report; new sensors take
        // precedence over periodic refresh. Each pose requires accepted registration.
        portENTER_CRITICAL(&m_mux);
        int selected = -1;
        for (int pass = 0; pass < 2 && selected < 0; pass++) {
            for (unsigned n = 0; n < 256; n++) {
                uint8_t hid = static_cast<uint8_t>(registrationCursor + n);
                int idx = findTracker(hid & 15);
                if (idx < 0 || !trackers[idx].online || !(trackers[idx].sensorMask & (1u << (hid >> 4)))) continue;
                if ((pass == 0 && !registeredHid[hid]) || (pass == 1 && now - lastInfoMs[hid] >= 100)) {
                    selected = hid; break;
                }
            }
        }
        if (selected >= 0) {
            uint8_t hid = static_cast<uint8_t>(selected);
            const auto &ti = trackers[findTracker(hid & 15)];
            uint8_t *reg = pendingReport;
            reg[0] = 255; reg[1] = hid;
            for (int b = 0; b < 6; b++) reg[2 + b] = ti.mac[5 - b];
            uint8_t sensor = hid >> 4;
            if (sensor > 0) { reg[7] = static_cast<uint8_t>(0x02 | (sensor << 4)); reg[2] += sensor; }
            uint8_t *info = pendingReport + HID_PACKET_SIZE;
            memset(info, 0, HID_PACKET_SIZE);
            info[0] = 0; info[1] = hid;
            info[2] = ti.batt; info[3] = ti.battV; info[4] = sensors[hid].hasTemperature ? sensors[hid].temp : 0;
            info[5] = ti.brdId; info[6] = ti.mcuId; info[8] = sensors[hid].hasInfo ? sensors[hid].imuId : 0; info[9] = sensors[hid].hasInfo ? sensors[hid].magId : 0;
            info[10] = ti.fwDate & 255; info[11] = ti.fwDate >> 8;
            info[12] = ti.fwMajor; info[13] = ti.fwMinor; info[14] = ti.fwPatch;
            info[15] = static_cast<uint8_t>(ti.rssi < 0 ? -ti.rssi : ti.rssi);
            registrationCursor = static_cast<uint8_t>(hid + 1);
            slot = 2;
        }
        portEXIT_CRITICAL(&m_mux);
        Packet status;
        while (slot < PACKETS_PER_REPORT && priorityPop(status)) {
            memcpy(pendingReport + slot++ * HID_PACKET_SIZE, status.data, HID_PACKET_SIZE);
        }
        portENTER_CRITICAL(&m_mux);
        for (unsigned n = 0; n < 256 && slot < PACKETS_PER_REPORT; n++) {
            uint8_t hid = static_cast<uint8_t>(poseCursor + n);
            int idx = findTracker(hid & 15);
            if (!poseDirty[hid] || !registeredHid[hid] || idx < 0 || !trackers[idx].online) continue;
            memcpy(pendingReport + slot * HID_PACKET_SIZE, latestPose[hid].data, HID_PACKET_SIZE);
            pendingVersion[slot++] = poseVersion[hid];
        }
        // Advance fairness cursor after the last pose included.
        for (size_t i = 0; i < slot; i++) if (pendingReport[i * HID_PACKET_SIZE] == 1)
            poseCursor = static_cast<uint8_t>(pendingReport[i * HID_PACKET_SIZE + 1] + 1);
        portEXIT_CRITICAL(&m_mux);
        if (slot == 0) return;
        pending = true;
    }

    // Replace poses on every retry. Critical records stay in the pending report
    // until accepted by the USB stack; they are never popped again on failure.
    portENTER_CRITICAL(&m_mux);
    for (size_t i = 0; i < PACKETS_PER_REPORT; i++) {
        uint8_t *record = pendingReport + i * HID_PACKET_SIZE;
        if (record[0] != 1) continue;
        uint8_t hid = record[1];
        int idx = findTracker(hid & 15);
        if (idx < 0 || !trackers[idx].online) { record[0] = 254; continue; }
        memcpy(record, latestPose[hid].data, HID_PACKET_SIZE);
        pendingVersion[i] = poseVersion[hid];
    }
    portEXIT_CRITICAL(&m_mux);
    bool accepted = hidDevice.send(pendingReport, HID_REPORT_SIZE);
    portENTER_CRITICAL(&m_mux);
    if (!accepted) {
        failedHidReports++;
        bool counted[MAX_TRACKERS] = {};
        for (size_t i = 0; i < PACKETS_PER_REPORT; i++) {
            uint8_t *record = pendingReport + i * HID_PACKET_SIZE;
            if (record[0] == 254) continue;
            int idx = findTracker(record[1] & 15);
            if (idx >= 0 && !counted[idx]) { trackers[idx].usbFailures++; counted[idx] = true; }
        }
    } else {
        successfulHidReports++;
        for (size_t i = 0; i < PACKETS_PER_REPORT; i++) {
            uint8_t *record = pendingReport + i * HID_PACKET_SIZE;
            uint8_t hid = record[1];
            if (record[0] == 255) { registeredHid[hid] = true; lastInfoMs[hid] = now; }
            if (record[0] == 1 && pendingVersion[i] == poseVersion[hid]) poseDirty[hid] = false;
        }
        pending = false;
    }
    portEXIT_CRITICAL(&m_mux);
}

PacketHandling PacketHandling::instance;
