

#include "HID.h"
#include "CdcConsoleStream.h"
#include "CommandConsole.h"
#include "WifiDongleConfig.h"
#include "button.h"
#include "configuration.h"
#include "error_codes.h"
#include "led.h"
#include "packetHandling.h"
#include "TrackerUdpReceiver.h"

#include <Arduino.h>
#include <USB.h>
#include <USBCDC.h>

USBCDC commandPort;
CdcConsoleStream consoleStream(commandPort);
HIDDevice hidDevice;
CommandConsole commandConsole(consoleStream, hidDevice);
Button &button = Button::getInstance();
LED led;

static char g_usbSerial[24];
static char g_apSsid[40];
static char g_apPassword[64];
static uint8_t g_apChannel;
static bool g_restartPending = false;
static uint32_t g_restartAt = 0;

static void buildIdentifiers() {

    uint64_t mac = ESP.getEfuseMac();
    uint8_t b3 = static_cast<uint8_t>((mac >> 24) & 0xFF);
    uint8_t b4 = static_cast<uint8_t>((mac >> 32) & 0xFF);
    uint8_t b5 = static_cast<uint8_t>((mac >> 40) & 0xFF);

    snprintf(g_usbSerial, sizeof(g_usbSerial), "SVRDG-%02X%02X%02X", b3, b4, b5);
    Configuration &configuration = Configuration::getInstance();
    snprintf(g_apSsid, sizeof(g_apSsid), "%s", configuration.getWifiSsid());
    snprintf(g_apPassword, sizeof(g_apPassword), "%s", configuration.getWifiPassword());
    g_apChannel = configuration.getWifiChannel();
}

TrackerUdpReceiver &receiver = TrackerUdpReceiver::getInstance();

[[noreturn]] void fail(ErrorCodes errorCode) {
    led.displayError(errorCode);
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting up " USB_PRODUCT "...");

    Configuration::getInstance().setup();
    buildIdentifiers();
    if (WifiDongleConfig::autoUniqueUsbSerial) {
        USB.serialNumber(g_usbSerial);
    }
    Serial.printf("[ID] USB serial: %s\n",
                  WifiDongleConfig::autoUniqueUsbSerial ? g_usbSerial : USB_SERIAL);
    Serial.printf("[ID] SoftAP SSID: %s  (password: %s)\n",
                  g_apSsid, g_apPassword);

    commandPort.begin(115200);
    commandPort.enableReboot(false);
    hidDevice.begin();
    USB.begin();
    commandConsole.begin(g_usbSerial, g_apSsid, g_apPassword, g_apChannel);

    button.begin();

    button.onMultiPress([](size_t pressCount) {
        if (pressCount == 5) {
            Serial.println("Trackers reset");
            Configuration::getInstance().resetTrackers();
            led.sendBlinks(5, 0.2f, 0.1f);
            g_restartPending = true;
            g_restartAt = millis() + 1600;
            return;
        }
    });

    led.begin();
    led.setState(false);

    ErrorCodes result = receiver.begin(
        g_apSsid,
        g_apPassword,
        g_apChannel,
        WifiDongleConfig::maxTrackers,
        WifiDongleConfig::apHidden
    );
    if (result != ErrorCodes::NO_ERROR) {
        fail(result);
    }

    receiver.onTrackerConnected(
        [&](uint8_t trackerId, const uint8_t *trackerMacAddress) {
            bool isNew = PacketHandling::getInstance().registerTracker(trackerId, trackerMacAddress);
            if (isNew) {
                Serial.println("New tracker connected");
                led.sendBlinks(2, 0.1f);
            }
        });

    Serial.println("Boot complete");
}

void loop() {
    button.update();
    led.update();
    receiver.update();
    PacketHandling::getInstance().tick(hidDevice);
    commandConsole.update();
    consoleStream.update();
    if (g_restartPending
        && static_cast<int32_t>(millis() - g_restartAt) >= 0) {
        ESP.restart();
    }
}
