/*
	SlimeVR ESP tracker WiFi dongle firmware.
*/

#include "HID.h"
#include "WifiDongleConfig.h"
#include "button.h"
#include "configuration.h"
#include "error_codes.h"
#include "led.h"
#include "packetHandling.h"
#include "SlimeServerEmu.h"
#include "logging/Logger.h"

#include <Arduino.h>
#include <USB.h>
#include <USBCDC.h>

HIDDevice hidDevice;
Button &button = Button::getInstance();
LED led;
SlimeVR::Logging::Logger logger("Main");

// 多台 dongle:用晶片 MAC 產生唯一的 USB 序號與(可選)SSID 後綴。
// 這兩個 buffer 要在整個執行期間有效(softAP / USB descriptor 會參照),故用 file-scope。
static char g_usbSerial[24];
static char g_apSsid[40];
static char g_apPassword[64];   // WPA2 密碼最長 63 字

// 追蹤器連上時要閃燈,但那個 callback 跑在 lwIP/AsyncUDP task 上,
// 而 LED 的狀態機是給 loop task 用的(led.update() 在 loop 裡跑),兩邊沒有互斥。
// 直接在 callback 裡呼叫 led.sendBlinks() 會和 led.update() 搶同一組成員變數:
// 輕則閃燈次數不對,重則 blink 計數被寫壞而一直閃個不停。
// 這裡只設旗標,真正的動作留給 loop()。volatile 讓編譯器不要把讀取最佳化掉。
static volatile bool g_newTrackerBlink = false;

static void buildIdentifiers() {
    // 注意:ESP.getEfuseMac() 的位元組順序是「反」的——它把 mac[0..5] 直接放進記憶體
    // 再以 little-endian uint64 讀出,所以 bit0-7 = mac[0](廠商前綴 OUI),
    // bit40-47 = mac[5](唯一序號尾碼)。要取「每台不同」的 byte 必須從高位取,
    // 否則同一批晶片會產生一模一樣的序號。
    uint64_t mac = ESP.getEfuseMac();
    uint8_t b2 = static_cast<uint8_t>((mac >> 16) & 0xFF);   // mac[2]
    uint8_t b3 = static_cast<uint8_t>((mac >> 24) & 0xFF);   // mac[3] ┐
    uint8_t b4 = static_cast<uint8_t>((mac >> 32) & 0xFF);   // mac[4] ├ 唯一尾碼
    uint8_t b5 = static_cast<uint8_t>((mac >> 40) & 0xFF);   // mac[5] ┘

    // USB 序號:固定前綴 + MAC 尾三碼 → 每台唯一、且同一台重開機後穩定。
    snprintf(g_usbSerial, sizeof(g_usbSerial), "SVRDG-%02X%02X%02X", b3, b4, b5);

    // SSID:預設沿用使用者設定的 apSsid;開啟 autoUniqueSsidSuffix 時才接上 MAC 尾碼。
    if (WifiDongleConfig::autoUniqueSsidSuffix) {
        snprintf(g_apSsid, sizeof(g_apSsid), "%s-%02X%02X",
                 WifiDongleConfig::apSsid, b4, b5);
    } else {
        snprintf(g_apSsid, sizeof(g_apSsid), "%s", WifiDongleConfig::apSsid);
    }

    // 密碼:預設沿用 apPassword;開啟 autoUniquePassword 時接上 4 個 MAC byte(8 hex)。
    // 用和 SSID 後綴「不同」的 byte(b2,b3),避免看到 SSID 就能直接推出密碼。
    if (WifiDongleConfig::autoUniquePassword) {
        snprintf(g_apPassword, sizeof(g_apPassword), "%s%02X%02X%02X%02X",
                 WifiDongleConfig::apPassword, b2, b3, b4, b5);
    } else {
        snprintf(g_apPassword, sizeof(g_apPassword), "%s", WifiDongleConfig::apPassword);
    }
}

SlimeServerEmu &comm = SlimeServerEmu::getInstance();

// 開機失敗後的狀態。原本這裡是 [[noreturn]] 的 fail():直接跳進 led.displayError()
// 的無限迴圈,永遠不返回 —— 於是 loop() 一次都不會跑。後果有兩個:
//  1. button.update() 不跑 → 五連按重設配對表這條唯一的救援路徑被封死,
//     使用者只剩「重新燒錄韌體」一途,而 LED 只是一直閃同一個碼。
//  2. SoftAP 啟動失敗、UDP 監聽失敗這類問題常常是暫時的(開機時序、NVS 忙碌),
//     重試一次就好了,但舊版永遠不會重試。
//     (順帶一提,原始碼裡這兩種失敗根本沒有自己的錯誤碼 ——
//      AP_START_FAILED / UDP_LISTEN_FAILED 是本補丁新增的,見 error_codes.h。)
// 改成:先阻塞閃三輪讓使用者讀得到錯誤碼,之後進 loop() 用慢閃表示「還沒起來」,
// 同時保持按鈕可用,並且每 10 秒自己重試一次 begin()。
static ErrorCodes g_bootError = ErrorCodes::NO_ERROR;
static uint32_t g_bootRetryAtMs = 0;
static constexpr uint32_t kBootRetryIntervalMs = 10000;

static ErrorCodes startComm() {
    return comm.begin(
        g_apSsid,
        g_apPassword,
        WifiDongleConfig::apChannel,
        WifiDongleConfig::maxTrackers,
        WifiDongleConfig::apHidden
    );
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting up " USB_PRODUCT "...");

    // USB 序號必須在 USB.begin() 之前設定(descriptor 只在列舉時送出一次)。
    buildIdentifiers();
    if (WifiDongleConfig::autoUniqueUsbSerial) {
        USB.serialNumber(g_usbSerial);
    }
    Serial.printf("[ID] USB serial: %s\n",
                  WifiDongleConfig::autoUniqueUsbSerial ? g_usbSerial : USB_SERIAL);
    // 密碼預設不印到序列埠:固定密碼在「編譯的當下」就會顯示在 build log 上
    // (見 print_wifi_config.py),序列埠即使平常沒人看,也不該常駐輸出憑證。
    // 唯一的例外是 autoUniquePassword:實際密碼含 MAC 後綴,只有執行期算得出來,
    // 不印的話使用者沒有任何地方查得到它。
    if (WifiDongleConfig::autoUniquePassword) {
        Serial.printf("[ID] SoftAP SSID: %s  (password: %s)\n",
                      g_apSsid, g_apPassword);
    } else {
        Serial.printf("[ID] SoftAP SSID: %s\n", g_apSsid);
    }

    Configuration::getInstance().setup();
    hidDevice.begin();
    USB.begin();

    // 注意:button.begin() 不在這裡,而在 setup() 的最後。
    // 它會掛上 GPIO 中斷,而中斷處理常式會 detachInterrupt() 並把狀態機切到
    // 輪詢模式,之後完全依賴 button.update() 推進 —— 但 setup() 後段的
    // led.displayErrorTimes() 會阻塞數秒,那段時間沒有人呼叫 update()。
    // 在那個空窗按下的按鍵不只是「晚一點才處理」,而是被吃掉:
    // update() 回來時已超過連按判定的 1 秒,pressCount 仍是 0,狀態機直接
    // 重新 attach。開機失敗時使用者最想做的正是五連按重設配對表。

    button.onMultiPress([](size_t pressCount) {
        if (pressCount == 5) {
            Serial.println("Trackers reset");

            // 順序:先閃燈回饋(1.2 秒)、再清表、清完立刻重開機。
            // 反過來寫(先清表、再閃 1.2 秒燈當回饋、最後才 restart)是不行的:
            // 那 1.2 秒裡 flash 上的配對表已經空了,而記憶體裡的 peer/tracker 表
            // 還是舊的,任何一顆新追蹤器在這段時間 handshake 都會拿到 id 0,
            // 撞到還在線的舊追蹤器並把它的 MAC 覆蓋掉。
            // 這是很容易發生的:使用者按完按鈕的下一個動作通常就是開新追蹤器電源。
            // 把回饋擺在清表之前,那個窗口就完全不存在。
            for (int i = 0; i < 5; i++) {
                led.setState(true);
                delay(120);
                led.setState(false);
                delay(120);
            }

            Configuration::getInstance().resetTrackers();

            // 清完配對表一定要重開機。
            // resetTrackers() 只刪掉 flash 上的對應表,記憶體裡的 peer 表(SlimeServerEmu)
            // 與 tracker 表(PacketHandling)都還留著舊的 trackerId。此時仍在線的追蹤器
            // 不會重新 handshake(靠 IP 就找得到),id 維持不變;而下一顆新連上的追蹤器
            // 會從空的配對表拿到 id 0 → 撞到還在線的舊追蹤器,registerTracker() 直接把
            // 舊的 MAC 覆蓋掉:舊追蹤器從 server 上消失,兩顆的資料混進同一個部位。
            // 重開機讓 flash 與記憶體狀態一致,是唯一不會留下殘留狀態的做法。
            Serial.println("Pairing table cleared, rebooting...");
            Serial.flush();

            ESP.restart();
            return;
        }
    });

    led.begin();
    led.setState(false);

    // 所有 callback 一定要在 comm.begin() 之前註冊完。
    // begin() 一回來,UDP socket 就已經在收封包了(AsyncUDP 有自己的 task),
    // 若這時 onTrackerConnected 還沒掛上,落在這個空窗裡的 handshake 就不會
    // 呼叫 registerTracker() —— 而 dongle 之後照樣回心跳,追蹤器認為連線正常
    // 不會重新握手,於是它在 SlimeVR Server 上永遠不出現,只能重開追蹤器電源。
    // 這個空窗雖然只有幾十毫秒,但「先開追蹤器再插 dongle」正好會踩中。
    comm.onTrackerConnected(
        [](uint8_t trackerId, const uint8_t *trackerMacAddress) {
            bool isNew = PacketHandling::getInstance().registerTracker(trackerId, trackerMacAddress);
            if (isNew) {
                Serial.println("New tracker connected");
                // 這裡是 lwIP task,不能直接碰 LED 狀態機(見檔頭 g_newTrackerBlink 說明)
                g_newTrackerBlink = true;
            }
        });

    ErrorCodes result = startComm();
    if (result != ErrorCodes::NO_ERROR) {
        Serial.printf("[Boot] comm.begin() failed with error code %u\n",
                      static_cast<unsigned>(result));
        Serial.flush();
        // 阻塞閃三輪。三輪足夠讀出錯誤碼,又不會久到讓使用者以為當機。
        // 這段期間按鈕中斷還沒掛上(button.begin() 在本函式最後),
        // 所以不會有「已經被 ISR 記下、卻沒有人 update()」的按鍵被吃掉。
        led.displayErrorTimes(result, 3);
        g_bootError = result;
        g_bootRetryAtMs = millis() + kBootRetryIntervalMs;
        // 之後改用非阻塞的慢閃:loop() 才跑得動,按鈕與重試都要靠它。
        led.sendContinuousBlinks(0.1f, 0.9f);
    } else {
        Serial.println("Boot complete");
    }

    // 最後才啟用按鈕:從這一行起 ISR 可能隨時觸發,而它之後的推進完全靠
    // loop() 裡的 button.update()。setup() 到此已經沒有任何阻塞動作。
    button.begin();
}

void loop() {
    if (g_bootError != ErrorCodes::NO_ERROR) {
        // 起不來也要讓 loop 轉:button.update() 靠連續輪詢做去彈跳與連按計數,
        // 停掉它等於停掉五連按重設。led.update() 維持慢閃。
        button.update();
        led.update();
        if (static_cast<int32_t>(millis() - g_bootRetryAtMs) >= 0) {
            g_bootRetryAtMs = millis() + kBootRetryIntervalMs;
            ErrorCodes retry = startComm();
            if (retry == ErrorCodes::NO_ERROR) {
                g_bootError = ErrorCodes::NO_ERROR;
                led.stopBlinking();
                led.setState(false);
                Serial.println("[Boot] comm.begin() succeeded on retry, boot complete");
            } else {
                Serial.printf("[Boot] retry failed with error code %u\n",
                              static_cast<unsigned>(retry));
            }
        }
        return;
    }

    // 把網路 task 設下的旗標在這裡(loop task)兌現成實際的 LED 動作。
    if (g_newTrackerBlink) {
        g_newTrackerBlink = false;
        led.sendBlinks(2, 0.1f);
    }

    button.update();
    led.update();
    comm.update();
    PacketHandling::getInstance().tick(hidDevice);
}
