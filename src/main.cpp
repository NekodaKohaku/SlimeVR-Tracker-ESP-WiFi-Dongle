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

static char g_usbSerial[24];
static char g_apSsid[40];
static char g_apPassword[64];

static volatile bool g_newTrackerBlink = false;

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

SlimeServerEmu &comm = SlimeServerEmu::getInstance();

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

    buildIdentifiers();
    if (WifiDongleConfig::autoUniqueUsbSerial) {
        USB.serialNumber(g_usbSerial);
    }
    Serial.printf("[ID] USB serial: %s\n",
                  WifiDongleConfig::autoUniqueUsbSerial ? g_usbSerial : USB_SERIAL);

    if (WifiDongleConfig::autoUniquePassword) {
        Serial.printf("[ID] SoftAP SSID: %s  (password: %s)\n",
                      g_apSsid, g_apPassword);
    } else {
        Serial.printf("[ID] SoftAP SSID: %s\n", g_apSsid);
    }

    Configuration::getInstance().setup();
    hidDevice.begin();
    USB.begin();

    button.onMultiPress([](size_t pressCount) {
        if (pressCount == 5) {
            Serial.println("Trackers reset");

            for (int i = 0; i < 5; i++) {
                led.setState(true);
                delay(120);
                led.setState(false);
                delay(120);
            }

            Configuration::getInstance().resetTrackers();

            Serial.println("Pairing table cleared, rebooting...");
            Serial.flush();

            ESP.restart();
            return;
        }
    });

    led.begin();
    led.setState(false);

    comm.onTrackerConnected(
        [](uint8_t trackerId, const uint8_t *trackerMacAddress) {
            bool isNew = PacketHandling::getInstance().registerTracker(trackerId, trackerMacAddress);
            if (isNew) {
                Serial.println("New tracker connected");

                g_newTrackerBlink = true;
            }
        });

    ErrorCodes result = startComm();
    if (result != ErrorCodes::NO_ERROR) {
        Serial.printf("[Boot] comm.begin() failed with error code %u\n",
                      static_cast<unsigned>(result));
        Serial.flush();

        led.displayErrorTimes(result, 3);
        g_bootError = result;
        g_bootRetryAtMs = millis() + kBootRetryIntervalMs;

        led.sendContinuousBlinks(0.1f, 0.9f);
    } else {
        Serial.println("Boot complete");
    }

    button.begin();
}

void loop() {
    if (g_bootError != ErrorCodes::NO_ERROR) {

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

    if (g_newTrackerBlink) {
        g_newTrackerBlink = false;
        led.sendBlinks(2, 0.1f);
    }

    button.update();
    led.update();
    comm.update();
    PacketHandling::getInstance().tick(hidDevice);
}
