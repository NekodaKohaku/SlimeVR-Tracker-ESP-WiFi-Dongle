#pragma once

#include <Arduino.h>
#include <Stream.h>
#include <USBCDC.h>

// Arduino-ESP32 suppresses CDC output while the host leaves DTR low.
// SlimeVR Server deliberately does that, so console output is queued here and
// drained through TinyUSB without blocking the tracker/HID loop.
class CdcConsoleStream : public Stream {
public:
    explicit CdcConsoleStream(USBCDC &cdc);

    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
    int availableForWrite() override;
    size_t write(uint8_t byte) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    using Print::write;

    void update();

private:
    static constexpr size_t txCapacity = 2048;
    static constexpr size_t maxDrainPerUpdate = 128;

    USBCDC &cdc;
    uint8_t txBuffer[txCapacity] = {0};
    size_t txHead = 0;
    size_t txTail = 0;
    size_t txCount = 0;
    bool lastOutputWasCarriageReturn = false;

    void enqueue(uint8_t byte);
};
