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
#include "logging/Logger.h"

#include <Arduino.h>
#include <USB.h>
#include <USBCDC.h>

#ifdef USE_OFFICIAL_PROXY
  #include "SlimeServerEmu.h"
#else
  #include "WifiCommunication.h"
#endif

HIDDevice hidDevice;
Button &button = Button::getInstance();
LED led;
SlimeVR::Logging::Logger logger("Main");

// 多台 dongle:用晶片 MAC 產生唯一的 USB 序號與(可選)SSID 後綴。
// 這兩個 buffer 要在整個執行期間有效(softAP / USB descriptor 會參照),故用 file-scope。
static char g_usbSerial[24];
static char g_apSsid[40];
static char g_apPassword[64];   // WPA2 密碼最長 63 字

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

#ifdef USE_OFFICIAL_PROXY
  SlimeServerEmu &comm = SlimeServerEmu::getInstance();
#else
  WifiCommunication &comm = WifiCommunication::getInstance();
#endif

[[noreturn]] void fail(ErrorCodes errorCode) {
    led.displayError(errorCode);
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
    Serial.printf("[ID] SoftAP SSID: %s  (password: %s)\n",
                  g_apSsid, g_apPassword);

    Configuration::getInstance().setup();
    hidDevice.begin();
    USB.begin();

    button.begin();

#ifndef USE_OFFICIAL_PROXY
    button.onLongPress([]() {
        if (!comm.isInPairingMode()) {
            Serial.println("Pairing mode enabled");
            comm.enterPairingMode();
            led.sendContinuousBlinks(0.1f, 0.5f);
        } else {
            Serial.println("Pairing mode disabled");
            comm.exitPairingMode();
            led.stopBlinking();
        }
    });
#endif

    button.onMultiPress([](size_t pressCount) {
        if (pressCount == 5) {
            Serial.println("Trackers reset");
            Configuration::getInstance().resetTrackers();
            led.sendBlinks(5, 0.2f, 0.1f);
            return;
        }
    });

    led.begin();
    led.setState(false);

    ErrorCodes result = comm.begin(
        g_apSsid,
        g_apPassword,
        WifiDongleConfig::apChannel,
        WifiDongleConfig::maxTrackers,
        WifiDongleConfig::apHidden
    );
    if (result != ErrorCodes::NO_ERROR) {
        fail(result);
    }

#ifndef USE_OFFICIAL_PROXY
    comm.onTrackerPaired([&]() {
        Serial.println("New tracker paired");
        led.sendBlinks(3, 0.1f);
    });
#endif

    comm.onTrackerConnected(
        [&](uint8_t trackerId, const uint8_t *trackerMacAddress) {
            bool isNew = PacketHandling::getInstance().registerTracker(trackerId, trackerMacAddress);
            if (isNew) {
                Serial.println("New tracker connected");
                led.sendBlinks(2, 0.1f);
            }
        });

#ifndef USE_OFFICIAL_PROXY
    comm.onInfoReceived(
        [&](const uint8_t *info) {
            PacketHandling::getInstance().insertInfo(info);
        });
    comm.onPacketReceived(
        [&](const uint8_t *packet) {
            PacketHandling::getInstance().insert(packet);
        });
#endif
    Serial.println("Boot complete");
}

void loop() {
    button.update();
    led.update();
    comm.update();
    PacketHandling::getInstance().tick(hidDevice);
}
