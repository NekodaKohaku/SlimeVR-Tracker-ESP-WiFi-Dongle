#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include <stdbool.h>

#define USB_VID          0x1209
#define USB_PID          0x7690
#define USB_MANUFACTURER "SlimeVR"
#define USB_PRODUCT      "SlimeVR WiFi Dongle"
#define USB_SERIAL       "SVRDGESP01B"

#define USB_FW_MSC_VENDOR_ID        "ESP32-S3"
#define USB_FW_MSC_PRODUCT_ID       "Firmware MSC"
#define USB_FW_MSC_PRODUCT_REVISION "1.23"
#define USB_FW_MSC_VOLUME_NAME      "S3-Firmware"
#define USB_FW_MSC_SERIAL_NUMBER    0x00000000

static const uint8_t LED_BUILTIN = 17;
#define BUILTIN_LED LED_BUILTIN
#define LED_BUILTIN LED_BUILTIN
static const bool LED_ACTIVE_LEVEL = true;

static const uint8_t USER_BUTTON = 0;
static const bool USER_BUTTON_ACTIVE_LEVEL = false;

#endif
