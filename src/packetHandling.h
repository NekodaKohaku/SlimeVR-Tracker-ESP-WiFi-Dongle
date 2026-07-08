#pragma once

#include "HID.h"

#include <Arduino.h>
#include <cstdint>
#include <cstring>

class PacketHandling {
public:
    static PacketHandling &getInstance();

    bool registerTracker(uint8_t trackerId, const uint8_t mac[6]);
    void insert(const uint8_t *payload);   // payload = 15 bytes
    void tick(HIDDevice &hidDevice);
    void insertInfo(const uint8_t *info);  // 舊的整包 14-byte info(保留相容)
    void updateRssiByMac(const uint8_t mac[6], int8_t rssi);

    // ---- 官方相容:拿到就更新對應欄位(透明代理用)----
    void setBattery(uint8_t trackerId, uint8_t pct, uint16_t mv);     // type 12
    void setTemp(uint8_t trackerId, uint8_t tempEncoded);            // type 20
    void setRssi(uint8_t trackerId, int8_t rssi);                    // type 19
    void setSensorInfo(uint8_t trackerId, uint8_t imuId, uint8_t magId);  // type 15
    void setFirmware(uint8_t trackerId, uint8_t brdId, uint8_t mcuId,
                     uint16_t fwDate, uint8_t fwMajor, uint8_t fwMinor, uint8_t fwPatch); // handshake
    void setTrackerOnline(uint8_t trackerId, bool online);  // 斷線/重連:送 status + 停/復送 register

private:
    static constexpr size_t HID_PACKET_SIZE = 16;
    static constexpr size_t PACKETS_PER_REPORT = 4;
    static constexpr size_t HID_REPORT_SIZE = HID_PACKET_SIZE * PACKETS_PER_REPORT; // 64
    static constexpr size_t MAX_TRACKERS = 16;
    static constexpr size_t FIFO_SIZE = 64;
    static constexpr size_t PRIORITY_FIFO_SIZE = 16;   // status/register 等高優先封包

    struct TrackerInfo {
        bool used = false;
        bool online = true;     // 斷線時設 false → 停送 register/device_info
        uint8_t id = 0;
        uint8_t mac[6] = {0};
        uint8_t batt = 0;
        uint8_t battV = 0;
        int8_t rssi = 0;
        uint8_t temp = 0;
        bool hasBattData = false;
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

    size_t regRotateIndex = 0;
    uint32_t lastRegSentMs = 0;

    // 跨 task spinlock:寫入(lwip task 的 onPacket->insert)與讀出(loop task 的 tick)
    // 同時操作 FIFO,必須鎖保護,否則高流量下指標競爭會掉包/亂序。
    portMUX_TYPE m_mux = portMUX_INITIALIZER_UNLOCKED;

    int findTracker(uint8_t id);
    void pushStatus(uint8_t trackerId, uint8_t status);
    bool fifoEmpty() const { return (fifoHead == fifoTail) && !fifoFull; }
    void fifoPush(const Packet &p, uint8_t trackerId);
    bool fifoPop(Packet &out);

    // 高優先 FIFO 操作
    bool priorityEmpty() const { return (priorityHead == priorityTail) && !priorityFull; }
    void priorityPush(const Packet &p);
    bool priorityPop(Packet &out);
};
