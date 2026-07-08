#pragma once

#include <USBHID.h>
#include <cstddef>
#include <cstdint>

// clang-format off
static const uint8_t hid_report_desc[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x00,        // Usage (Undefined)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x00,        // Usage (Undefined)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x40,        //   Report Count (64)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection
};
// clang-format on

class HIDDevice : public USBHIDDevice {
public:
    HIDDevice();
    void begin();
    uint16_t _onGetDescriptor(uint8_t *buffer);
    bool send(const uint8_t *value, size_t size);
    bool ready();

private:
    static bool initialized;
    USBHID HID;
};
