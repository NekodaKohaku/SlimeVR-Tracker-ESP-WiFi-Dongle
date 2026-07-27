

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

static char g_usbSerial[24];
static char g_apSsid[40];
static char g_apPassword[64];

static void buildIdentifiers() {

    uint64_t mac = ESP.getEfuseMac();
    uint8_t b2 = static_cast<uint8_t>((mac >> 16) & 0xFF);
    uint8_t b3 = static_cast<uint8_t>((mac >> 24) & 0xFF);
    uint8_t b4 = static_cast<uint8_t>((mac >> 32) & 0xFF);
    uint8_t b5 = static_cast<uint8_t>((mac >> 40) & 0xFF);

    snprintf(g_usbSerial, sizeof(g_usbSerial), "SVRDG-%02X%02X%02X", b3, b4, b5);

    if (WifiDongleConfig::autoUniqueSsidSuffix) {
        snprintf(g_apSsid, sizeof(g_apSsid), "%s-%02X%02X",
                 WifiDongleConfig::apSsid, b4, b5);
    } else {
        snprintf(g_apSsid, sizeof(g_apSsid), "%s", WifiDongleConfig::apSsid);
    }

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
