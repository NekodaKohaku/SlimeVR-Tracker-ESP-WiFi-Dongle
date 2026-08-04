#pragma once

#include "HID.h"

#include <Arduino.h>
#include <Stream.h>

class CommandConsole {
public:
    CommandConsole(Stream &port, HIDDevice &hidDevice);
    void begin(const char *usbSerial, const char *activeSsid,
               const char *activePassword, uint8_t activeChannel);
    void update();

private:
    static constexpr size_t lineCapacity = 160;

    Stream &port;
    HIDDevice &hidDevice;
    char line[lineCapacity] = {0};
    size_t lineLength = 0;
    bool discardingLine = false;
    const char *usbSerial = nullptr;
    const char *activeSsid = nullptr;
    const char *activePassword = nullptr;
    uint8_t activeChannel = 0;

    void execute(char *command);
    void printHelp();
    void printInfo();
    void printStatus();
    void printWifi();
    void printMeow();
    void setWifi(const char *field, char *value);
    bool wifiRestartRequired() const;
    static char *trim(char *text);
    static char *valueWithoutOptionalQuotes(char *text);
};
