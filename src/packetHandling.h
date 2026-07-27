#pragma once

#include "HID.h"
#include "WifiDongleConfig.h"

#include <Arduino.h>
#include <cstdint>
#include <cstring>

static_assert(WifiDongleConfig::maxTrackers >= 1 && WifiDongleConfig::maxTrackers <= 10,
              "WifiDongleConfig::maxTrackers must be between 1 and 10");

class PacketHandling {
public:
    static PacketHandling &getInstance();

    bool registerTracker(uint8_t trackerId, const uint8_t mac[6]);
    void insert(const uint8_t *payload);
    void tick(HIDDevice &hidDevice);

    void setBattery(uint8_t trackerId, uint8_t pct, uint16_t mv);

    void setTemp(uint8_t trackerId, uint8_t sensorId, uint8_t tempEncoded);
    void setRssi(uint8_t trackerId, int8_t rssi);
    void setSensorInfo(uint8_t trackerId, uint8_t sensorId, uint8_t imuId, uint8_t magId);
    void setFirmware(uint8_t trackerId, uint8_t brdId, uint8_t mcuId,
                     uint16_t fwDate, uint8_t fwMajor, uint8_t fwMinor, uint8_t fwPatch);
    void setTrackerOnline(uint8_t trackerId, bool online);

private:
    static constexpr size_t HID_PACKET_SIZE = 16;
    static constexpr size_t PACKETS_PER_REPORT = 4;
    static constexpr size_t HID_REPORT_SIZE = HID_PACKET_SIZE * PACKETS_PER_REPORT;
    static constexpr size_t MAX_TRACKERS = 16;
    static constexpr size_t FIFO_SIZE = 64;
    static constexpr size_t PRIORITY_FIFO_SIZE = 32;

    static constexpr size_t MAX_SENSORS = 16;

    static constexpr uint8_t HID_FILLER_ID = 0xFF;

    struct TrackerInfo {
        bool used = false;
        bool online = true;
        uint8_t id = 0;
        uint16_t sensorMask = 1;
        uint8_t mac[6] = {0};
        uint8_t batt = 0;
        uint8_t battV = 0;
        int8_t rssi = 0;
        bool hasBattData = false;
        uint8_t brdId = 0;
        uint8_t mcuId = 0;

        uint8_t temp[MAX_SENSORS] = {};
        uint8_t imuId[MAX_SENSORS] = {};
        uint8_t magId[MAX_SENSORS] = {};

        uint16_t imuKnownMask = 0;

        uint32_t sensorSeenMs[MAX_SENSORS] = {};

        uint8_t defaultImuId = 0;
        uint8_t defaultMagId = 0;
        uint16_t fwDate = 0;
        uint8_t fwMajor = 0;
        uint8_t fwMinor = 0;
        uint8_t fwPatch = 0;

        uint8_t graceRearms = 0;

        uint32_t lastGraceArmMs = 0;
    };

    struct Packet { uint8_t data[HID_PACKET_SIZE]; };

    PacketHandling() = default;
    static PacketHandling instance;

    TrackerInfo trackers[MAX_TRACKERS];

    Packet fifo[FIFO_SIZE];
    size_t fifoHead = 0;
    size_t fifoTail = 0;
    bool fifoFull = false;

    Packet priorityFifo[PRIORITY_FIFO_SIZE];
    size_t priorityHead = 0;
    size_t priorityTail = 0;
    bool priorityFull = false;

    uint32_t priorityTailSeq = 0;

    size_t regRotateIndex = 0;
    uint32_t lastRegSentMs = 0;

    uint32_t priorityDropped = 0;
    uint32_t priorityDroppedLogged = 0;

    static constexpr uint32_t SENSOR_INFO_GRACE_MS = 2000;

    static constexpr uint8_t MAX_GRACE_REARMS = 5;

    static constexpr uint32_t GRACE_EPISODE_MS = 10000;

    portMUX_TYPE m_mux = portMUX_INITIALIZER_UNLOCKED;

    int findTracker(uint8_t id);

    void pushStatusHid(uint8_t hidId, uint8_t status);
    bool fifoEmpty() const { return (fifoHead == fifoTail) && !fifoFull; }
    void fifoPush(const Packet &p, uint8_t hidId);
    bool fifoPop(Packet &out);

    bool priorityEmpty() const { return (priorityHead == priorityTail) && !priorityFull; }
    void priorityPush(const Packet &p);

    bool priorityPeek(size_t offset, Packet &out, uint32_t &tailSeen);

    void priorityDrop(size_t count, uint32_t tailSeen);
};
