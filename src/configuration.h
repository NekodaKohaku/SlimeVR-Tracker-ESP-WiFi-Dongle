#pragma once

#include "WifiDongleConfig.h"

#include <LittleFS.h>
#include <cstdint>

// 配對結果分三種,呼叫端必須分得出來:
//  - Ok        : trackerId 有效。
//  - Permanent : 表滿了(或表上這筆的 id 越界)。本次開機內不可能自己好轉 ——
//                配對表只有五連按會清除,而五連按接著就重開機。
//  - Transient : flash 寫入/開檔失敗。下一次呼叫可能就成功了。
// 分開的理由:呼叫端會把「被拒絕的 MAC」快取起來避免每秒重掃 flash(那個掃描
// 跑在 lwIP task 上會阻塞網路堆疊)。若把 Transient 也快取,一次偶發的寫入失敗
// 就會讓那顆追蹤器在本次開機內永遠連不上,而使用者完全看不出原因。
enum class TrackerIdStatus : uint8_t { Ok, Permanent, Transient };

class Configuration {
public:
    static Configuration &getInstance();
    void setup();
    uint8_t getSavedTrackerCount();
    void setSavedTrackerCount(uint8_t newValue);
    TrackerIdStatus getOrCreateTrackerId(const uint8_t mac[6], uint8_t &trackerId);
    void resetTrackers();

private:
    Configuration() = default;

    static Configuration instance;

    static constexpr char savedTrackerCountPath[] = "/savedTrackerCount.bin";
    static constexpr char trackerMapPath[] = "/trackerMap.bin";
    // 修復配對表時的暫存檔:先寫它、確認成功才蓋回去,失敗時原檔完好如初。
    static constexpr char trackerMapTmpPath[] = "/trackerMap.tmp";
    static constexpr uint8_t maxTrackers = WifiDongleConfig::maxTrackers;

    // 配對表一筆的大小:6 byte MAC + 1 byte id。
    static constexpr size_t mapRecordSize = 7;

    // LittleFS 掛載成功了嗎。setup() 失敗時原本沒有人記錄這件事,
    // 之後每次 getOrCreateTrackerId() 都會走「檔案不存在 → 配 id 0」這條路,
    // 於是「每一顆」追蹤器都拿到 id 0:全部擠進同一個部位、互相覆蓋 MAC,
    // 而序列埠上完全看不出哪裡不對。
    bool fsReady = false;

    // LittleFS 不可用時的記憶體後備配對表。重開機就沒了(本來就沒得存),
    // 但至少在本次開機內每顆追蹤器仍拿到不同且穩定的 id。
    struct RamEntry { uint8_t mac[6]; uint8_t id; bool used; };
    RamEntry ramMap[maxTrackers] = {};

    // 連續這麼多次 Transient 就認定 flash 不可用,改走記憶體後備表。
    // 不設上限的話,一顆配不到 id 的追蹤器會每秒逼出一次 flash 寫入失敗 ——
    // 而那些寫入跑在 lwIP task 上,會連帶把其他正常的追蹤器拖到逾時。
    static constexpr uint8_t maxTransientFailures = 3;
    uint8_t transientFailures = 0;

    TrackerIdStatus getOrCreateTrackerIdRam(const uint8_t mac[6], uint8_t &trackerId);
    TrackerIdStatus getOrCreateTrackerIdFs(const uint8_t mac[6], uint8_t &trackerId);

    // 把長度不是 mapRecordSize 倍數的配對表截回對齊,成功回 true。
    bool repairTrackerMapAlignment();
    // 開機時接回上次中斷的修復(見 .cpp)。
    void recoverPendingRepair();
};
