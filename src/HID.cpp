#include "HID.h"

#include <cstring>

HIDDevice::HIDDevice() {
    if (initialized) {
        return;
    }

    initialized = true;
    HID.addDevice(this, sizeof(hid_report_desc));
}

void HIDDevice::begin() {
    HID.begin();
}

uint16_t HIDDevice::_onGetDescriptor(uint8_t *buffer) {
    memcpy(buffer, hid_report_desc, sizeof(hid_report_desc));
    return sizeof(hid_report_desc);
}

bool HIDDevice::send(const uint8_t *value, size_t size) {
    return HID.SendReport(0, value, size);
}

bool HIDDevice::ready() {
    return HID.ready();
}

bool HIDDevice::initialized = false;
