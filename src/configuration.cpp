#include "configuration.h"

#include <cstring>

Configuration &Configuration::getInstance() {
    return instance;
}

void Configuration::setup() {
    fsReady = false;

    bool status = LittleFS.begin();
    if (!status) {
        Serial.println("Could not mount LittleFS, formatting");

        status = LittleFS.format();
        if (!status) {
            Serial.println("Could not format LittleFS, aborting");
            return;
        }

        status = LittleFS.begin();
        if (!status) {
            Serial.println("Could not mount LittleFS, aborting");
            return;
        }
    }
    fsReady = true;
    transientFailures = 0;
    recoverPendingRepair();
    Serial.println("LittleFS is mounted");
}

uint8_t Configuration::getSavedTrackerCount() {
    if (!fsReady) return 0;
    if (!LittleFS.exists(savedTrackerCountPath)) {
        Serial.printf("%s doesn't exist, returning 0 saved trackers\n", savedTrackerCountPath);
        return 0;
    }

    auto file = LittleFS.open(savedTrackerCountPath, "r");
    if (!file) return 0;   // 檔案存在不代表開得起來(壞掉的 FS / 資源不足)
    uint8_t result = 0;
    if (file.read(&result, sizeof(uint8_t)) != sizeof(uint8_t)) result = 0;
    file.close();

    return result;
}

void Configuration::setSavedTrackerCount(uint8_t newValue) {
    if (!fsReady) return;
    Serial.printf("New saved trackers count: %d\n", newValue);
    auto file = LittleFS.open(savedTrackerCountPath, "w", true);
    if (!file) {
        Serial.println("Could not open savedTrackerCount for write");
        return;
    }
    file.write(&newValue, sizeof(uint8_t));
    file.close();
}

// LittleFS 不可用時的後備:配對只存在記憶體裡。
// 不做這個的話,原本的流程會在每次呼叫都認為「檔案不存在 → usedMask=0 → 配 id 0」,
// 結果所有追蹤器共用 id 0,在 SlimeVR Server 上全部疊在同一個部位上。
TrackerIdStatus Configuration::getOrCreateTrackerIdRam(const uint8_t mac[6], uint8_t &trackerId) {
    for (uint8_t i = 0; i < maxTrackers; i++) {
        if (ramMap[i].used && memcmp(ramMap[i].mac, mac, 6) == 0) {
            trackerId = ramMap[i].id;
            return TrackerIdStatus::Ok;
        }
    }
    for (uint8_t i = 0; i < maxTrackers; i++) {
        if (ramMap[i].used) continue;
        memcpy(ramMap[i].mac, mac, 6);
        ramMap[i].id = i;
        ramMap[i].used = true;
        trackerId = i;
        return TrackerIdStatus::Ok;
    }
    return TrackerIdStatus::Permanent;   // 記憶體表也滿了,同樣要五連按才會變
}

// 配對表是 7-byte 記錄的連續串流。若追加一筆時第二個 byte 沒寫進去(flash 滿、
// 斷電),檔案長度就停在 7 的非倍數上。這種壞法特別惡劣:下次開機照樣讀得下去,
// 只是從那一筆之後每一次 read 都跨在兩筆記錄的邊界上 —— 讀出來的「MAC」是前一筆
// 的尾巴接後一筆的頭,於是整張表全錯,所有追蹤器都配到別人的 id,使用者只看到
// 部位設定莫名其妙全亂,而序列埠上一個錯誤訊息都不會有。
//
// 現在這種壞法在本檔案裡已經不可能自己產生了:append 改成「一次寫滿 7 byte」
// (見 getOrCreateTrackerId)。這個函式留著是為了修掉舊版本韌體留下的、
// 或斷電造成的既有壞表。
//
// 修法:只保留前面完整的記錄,把尾巴那半筆丟掉(那顆追蹤器下次握手會重新配一個 id)。
//
// 【務必經由暫存檔】直接 open(trackerMapPath, "w") 會「先把原檔清成 0 byte」,
// 之後才輪到 write —— 而會走到這裡的前提往往正是 flash 出了問題,那個 write
// 很可能也會失敗。那樣就從「表的尾巴壞了一筆」升級成「整張表沒了」:
// 所有追蹤器的部位設定一次全毀。先寫暫存檔、確認寫成功才 rename 蓋過去,
// 任何一步失敗原檔都原封不動。
bool Configuration::repairTrackerMapAlignment() {
    uint8_t buf[maxTrackers * mapRecordSize];
    size_t keep = 0;
    size_t sz = 0;
    {
        auto file = LittleFS.open(trackerMapPath, "r");
        if (!file) return false;
        sz = file.size();
        keep = sz - (sz % mapRecordSize);
        // 超長的表不在這裡處理:截斷會刪掉使用者的配對。
        // 呼叫端只讀前面 maxTrackers 筆就好(見 getOrCreateTrackerId)。
        if (keep > sizeof(buf)) keep = sizeof(buf);
        if (keep > 0 && file.read(buf, keep) != keep) {
            file.close();
            return false;
        }
        file.close();
    }
    Serial.printf("trackerMap misaligned (%u bytes), keeping %u\n",
                  static_cast<unsigned>(sz), static_cast<unsigned>(keep));

    if (keep == 0) {
        // 一筆完整的都沒有,沒有東西可救,直接刪掉重來。
        return LittleFS.remove(trackerMapPath);
    }

    auto out = LittleFS.open(trackerMapTmpPath, "w", true);
    if (!out) return false;
    bool ok = (out.write(buf, keep) == keep);
    out.close();
    if (!ok) {
        LittleFS.remove(trackerMapTmpPath);   // 原檔沒被動過
        return false;
    }
    // rename 前先刪掉目的檔。LittleFS 的 rename 其實允許覆寫,但這裡不依賴那個語意:
    // 覆寫與否是底層實作的細節(不同版本、不同 FS 後端不一定一致),而這一步做錯的
    // 代價是整張配對表的去留。先刪再改名,兩種語意下的結果都相同,而且刪除失敗時
    // 還能在原檔完好的狀態下退出。
    // (走到這裡時 trackerMapPath 必定存在 —— 函式開頭剛以 "r" 開過它,
    //  所以這個 remove 不會因為「檔案不存在」而失敗。)
    if (!LittleFS.remove(trackerMapPath)) {
        LittleFS.remove(trackerMapTmpPath);
        return false;
    }
    if (!LittleFS.rename(trackerMapTmpPath, trackerMapPath)) {
        // 這裡原檔已經沒了,但完整的資料還在暫存檔上 —— 下次開機的
        // recoverPendingRepair() 會把它接回去,不會整張表消失。
        Serial.println("trackerMap rename FAILED (data is in the temp file)");
        return false;
    }
    return true;
}

// 開機時把上一次修復到一半(已刪原檔、還沒 rename 成功)的暫存檔接回去。
// 沒有這一步的話,那個窗口內斷電就等於整張配對表消失。
void Configuration::recoverPendingRepair() {
    if (!LittleFS.exists(trackerMapTmpPath)) return;
    if (LittleFS.exists(trackerMapPath)) {
        // 原檔還在 = 上次是寫暫存檔那一步失敗的,暫存檔內容不可信,丟掉。
        LittleFS.remove(trackerMapTmpPath);
        return;
    }
    Serial.println("recovering trackerMap from interrupted repair");
    LittleFS.rename(trackerMapTmpPath, trackerMapPath);
}

// 這一層只負責「連續 Transient 就退回記憶體表」。
//
// 為什麼需要:Transient 是刻意不被呼叫端快取的(見 configuration.h),所以追蹤器
// 每秒都會再問一次。若 flash 是真的壞了/滿了,那就是每秒一次「掃表 + 開檔 + 寫失敗」
// 跑在 lwIP task 上 —— LittleFS 的寫入會觸發 erase,一次可以塞住網路堆疊幾十到
// 上百毫秒。結果是一顆配不到 id 的追蹤器,把其他正常運作的追蹤器一起拖到逾時。
// 連續失敗到一定次數就認定 flash 不可用,改走記憶體後備表:此後不再碰 flash,
// 每顆追蹤器仍拿得到不同且本次開機內穩定的 id。
TrackerIdStatus Configuration::getOrCreateTrackerId(const uint8_t mac[6], uint8_t &trackerId) {
    if (!fsReady) return getOrCreateTrackerIdRam(mac, trackerId);

    TrackerIdStatus st = getOrCreateTrackerIdFs(mac, trackerId);
    if (st == TrackerIdStatus::Transient) {
        if (++transientFailures >= maxTransientFailures) {
            Serial.println("trackerMap storage keeps failing, falling back to RAM pairing");
            fsReady = false;
            return getOrCreateTrackerIdRam(mac, trackerId);
        }
    } else {
        transientFailures = 0;
    }
    return st;
}

TrackerIdStatus Configuration::getOrCreateTrackerIdFs(const uint8_t mac[6], uint8_t &trackerId) {
    // 一次掃完就同時做兩件事:找這顆 MAC、統計已用過的 id。
    // 原本掃兩遍,除了多一次 flash 讀取,兩遍之間的狀態也可能不一致。
    uint16_t usedMask = 0;
    if (LittleFS.exists(trackerMapPath)) {
        {
            // 先確認記錄邊界沒跑掉,再開始逐筆讀(理由見 repairTrackerMapAlignment)。
            //
            // 只有「長度不是 7 的倍數」才算壞掉。長度超過 maxTrackers 筆不算 ——
            // 使用者把 maxTrackers 從 10 改小再燒進去時,原本合法的 70-byte 表就會
            // 超過上限,若在這裡截斷,四顆追蹤器的配對就永久消失了(而且序列埠上
            // 還會謊稱「表損壞」)。超長的表照樣可以掃,只是掃出來的越界 id 用不了。
            auto probe = LittleFS.open(trackerMapPath, "r");
            if (!probe) return TrackerIdStatus::Transient;
            size_t sz = probe.size();
            bool aligned = (sz % mapRecordSize == 0);
            probe.close();
            if (!aligned && !repairTrackerMapAlignment()) {
                Serial.println("trackerMap repair FAILED");
                return TrackerIdStatus::Transient;
            }
        }
        auto file = LittleFS.open(trackerMapPath, "r");
        if (file) {
            uint8_t rec[mapRecordSize];
            while (file.read(rec, mapRecordSize) == mapRecordSize) {
                const uint8_t *storedMac = rec;
                uint8_t storedId = rec[6];
                if (memcmp(storedMac, mac, 6) == 0) {
                    trackerId = storedId;
                    file.close();
                    // 表上這筆的 id 越界 = 這筆對現在的 maxTrackers 沒有意義。
                    // 重掃也不會變好(只有五連按會清),所以歸類為 Permanent,
                    // 讓呼叫端快取、不要每秒再掃一次 flash。
                    return storedId < maxTrackers ? TrackerIdStatus::Ok
                                                  : TrackerIdStatus::Permanent;
                }
                if (storedId < maxTrackers) {
                    if (usedMask & (1u << storedId)) {
                        // 同一個 id 出現兩次 = 表的內容壞了(不是長度壞)。
                        // 這裡不動 flash:無法判斷哪一筆才是對的,而重寫表的風險
                        // 遠大於收益。只留下紀錄,並靠 registerTracker() 那邊
                        // 「同一個 id 換 MAC 要有條件」的檢查擋住實際傷害。
                        Serial.printf("trackerMap has duplicate id %u (table is stale)\n",
                                      static_cast<unsigned>(storedId));
                    }
                    usedMask |= static_cast<uint16_t>(1u << storedId);
                }
            }
            file.close();
        }
    }

    for (uint8_t id = 0; id < maxTrackers; id++) {
        if ((usedMask & (1u << id)) != 0) {
            continue;
        }

        // 一次寫滿整筆 7 byte。
        // 原本是「先寫 6 byte MAC、再寫 1 byte id」兩次獨立寫入:第二次短寫的話,
        // 前 6 個 byte 已經確定寫進去了 —— 這個函式回報失敗,卻同時在 flash 上
        // 留下半筆記錄,把整張表的邊界推歪(後果見 repairTrackerMapAlignment)。
        // 也就是說,舊寫法自己就是那個「壞表」的來源。
        uint8_t rec[mapRecordSize];
        memcpy(rec, mac, 6);
        rec[6] = id;

        // 寫入必須確認成功。flash 滿了或 FS 損壞時 open/write 會失敗,
        // 原本直接忽略回傳值就回 true:呼叫端以為配好了,重開機後這顆 MAC
        // 不在表上,又會拿到別的 id → 追蹤器的部位設定每次開機都跑掉。
        auto file = LittleFS.open(trackerMapPath, "a", true);
        if (!file) {
            Serial.println("Could not open trackerMap for append");
            return TrackerIdStatus::Transient;
        }
        bool ok = (file.write(rec, mapRecordSize) == mapRecordSize);
        file.close();
        if (!ok) {
            Serial.println("Could not write trackerMap entry");
            return TrackerIdStatus::Transient;
        }

        trackerId = id;
        // 這裡原本還會 setSavedTrackerCount(id + 1)。拿掉了:整棵樹裡沒有任何一處
        // 讀 getSavedTrackerCount(),那個檔案是只寫不讀的。而它的代價很實在 ——
        // 每配一顆新追蹤器就多一次 open("w")+write(等於一次 flash erase),
        // 而且同樣跑在 lwIP task 上。resetTrackers() 仍會把它歸零,舊韌體留下的
        // 檔案不會變成孤兒。
        return TrackerIdStatus::Ok;
    }

    return TrackerIdStatus::Permanent;   // 0..maxTrackers-1 全被佔用
}

void Configuration::resetTrackers() {
    // 記憶體後備表也要清,否則 LittleFS 不可用時五連按等於沒作用。
    memset(ramMap, 0, sizeof(ramMap));
    if (!fsReady) return;
    setSavedTrackerCount(0);
    if (LittleFS.exists(trackerMapPath)) {
        LittleFS.remove(trackerMapPath);
    }
    if (LittleFS.exists(trackerMapTmpPath)) {
        LittleFS.remove(trackerMapTmpPath);
    }
    transientFailures = 0;
}

Configuration Configuration::instance;
