#pragma once

#include "HID.h"

#include <Arduino.h>
#include <cstdint>
#include <cstring>

class PacketHandling {
public:
    struct Stats {
        uint32_t droppedPackets;
        uint32_t failedHidReports;
        uint32_t coalescedPoses;
        uint32_t successfulHidReports;
        uint32_t rejectedUdpPackets;
    };

    static PacketHandling &getInstance();

    bool registerTracker(uint8_t trackerId, const uint8_t mac[6]);
    void insert(const uint8_t *payload);
    void tick(HIDDevice &hidDevice);
    void insertInfo(const uint8_t *info);
    void updateRssiByMac(const uint8_t mac[6], int8_t rssi);

    void setBattery(uint8_t trackerId, uint8_t pct, uint16_t mv);
    void setTemp(uint8_t trackerId, uint8_t tempEncoded, uint8_t sensorId = 0);
    void setRssi(uint8_t trackerId, int8_t rssi);
    void setSensorInfo(uint8_t trackerId, uint8_t imuId, uint8_t magId, uint8_t sensorId = 0, int sensorState = -1);
    void setFirmware(uint8_t trackerId, uint8_t brdId, uint8_t mcuId,
                     uint16_t fwDate, uint8_t fwMajor, uint8_t fwMinor, uint8_t fwPatch);
    void setTrackerOnline(uint8_t trackerId, bool online);
    Stats getStats();
    void recordReception(uint8_t trackerId);
    void recordRejectedUdp(uint8_t trackerId = 255);
    void printDiagnostics(Print &out);
    void printSensors(Print &out, uint8_t trackerId);
    void forgetTracker(uint8_t trackerId);

private:
    static constexpr size_t HID_PACKET_SIZE = 16;
    static constexpr size_t PACKETS_PER_REPORT = 4;
    static constexpr size_t HID_REPORT_SIZE = HID_PACKET_SIZE * PACKETS_PER_REPORT;
    static constexpr size_t MAX_TRACKERS = 16;
    static constexpr size_t PRIORITY_FIFO_SIZE = 32;

    struct SensorInfo {
        bool hasPose = false, hasTemperature = false, hasInfo = false;
        uint32_t lastPoseMs = 0, poses = 0;
        uint8_t temp = 0, imuId = 0, magId = 0;
        int sensorState = -1;
    };
    SensorInfo sensors[256] = {};
    struct TrackerInfo {
        bool used = false;
        bool online = true;
        uint8_t id = 0;
        uint16_t sensorMask = 1;
        uint8_t mac[6] = {0};
        uint8_t batt = 0;
        uint8_t battV = 0;
        int8_t rssi = 0;
        uint8_t temp = 0;
        bool hasBattData = false;
        uint32_t received = 0;
        uint32_t lastRxMs = 0;
        uint32_t rateStartMs = 0;
        uint32_t rateCount = 0;
        uint32_t pps = 0;
        uint32_t coalesced = 0;
        uint32_t usbFailures = 0;
        uint32_t rejectedUdp = 0;
        uint32_t controlDrops = 0;
        uint8_t brdId = 0;
        uint8_t mcuId = 0;
        uint8_t imuId = 0;
        uint8_t magId = 0;
        uint16_t fwDate = 0;
        uint8_t fwMajor = 0;
        uint8_t fwMinor = 0;
        uint8_t fwPatch = 0;
    };

    struct Packet { uint8_t data[HID_PACKET_SIZE]; };

    PacketHandling() = default;
    static PacketHandling instance;

    TrackerInfo trackers[MAX_TRACKERS];

    Packet priorityFifo[PRIORITY_FIFO_SIZE];
    size_t priorityHead = 0;
    size_t priorityTail = 0;
    bool priorityFull = false;

    // Latest pose per HID ID, shared by UDP producer and USB consumer.
    Packet latestPose[256] = {};
    uint32_t poseVersion[256] = {};
    bool poseDirty[256] = {};
    bool registeredHid[256] = {};
    uint32_t lastInfoMs[256] = {};
    uint8_t poseCursor = 0;
    uint8_t registrationCursor = 0;
    uint32_t lastSubmitMs = 0;
    bool pending = false;
    uint8_t pendingReport[HID_REPORT_SIZE] = {};
    uint32_t pendingVersion[PACKETS_PER_REPORT] = {};
    uint32_t coalescedPoses = 0;
    uint32_t successfulHidReports = 0;
    uint32_t rejectedUdpPackets = 0;
    uint32_t droppedPackets = 0;
    uint32_t failedHidReports = 0;

    portMUX_TYPE m_mux = portMUX_INITIALIZER_UNLOCKED;

    static constexpr size_t MAX_SENSORS = 16;

    int findTracker(uint8_t id);

    void pushStatusHid(uint8_t hidId, uint8_t status);
    bool priorityEmpty() const { return (priorityHead == priorityTail) && !priorityFull; }
    void priorityPush(const Packet &p);
    bool priorityPop(Packet &out);
};
