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
        WifiDongleConfig::apSsid,
        WifiDongleConfig::apPassword,
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
