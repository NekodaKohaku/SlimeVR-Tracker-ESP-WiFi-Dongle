#include "CdcConsoleStream.h"

#include <esp32-hal-tinyusb.h>

CdcConsoleStream::CdcConsoleStream(USBCDC &newCdc) : cdc(newCdc) {}

int CdcConsoleStream::available() {
    return cdc.available();
}

int CdcConsoleStream::read() {
    return cdc.read();
}

int CdcConsoleStream::peek() {
    return cdc.peek();
}

void CdcConsoleStream::flush() {
    update();
}

int CdcConsoleStream::availableForWrite() {
    // A bare LF may expand to CRLF, so report the conservative capacity.
    return static_cast<int>((txCapacity - txCount) / 2);
}

size_t CdcConsoleStream::write(uint8_t byte) {
    return write(&byte, 1);
}

size_t CdcConsoleStream::write(const uint8_t *buffer, size_t size) {
    if (buffer == nullptr || size == 0) return 0;

    size_t written = 0;
    while (written < size) {
        uint8_t byte = buffer[written];
        bool needsCarriageReturn = byte == '\n' && !lastOutputWasCarriageReturn;
        size_t required = needsCarriageReturn ? 2 : 1;
        if (txCapacity - txCount < required) break;

        if (needsCarriageReturn) enqueue('\r');
        enqueue(byte);
        lastOutputWasCarriageReturn = byte == '\r';
        written++;
    }
    return written;
}

void CdcConsoleStream::enqueue(uint8_t byte) {
    txBuffer[txHead] = byte;
    txHead = (txHead + 1) % txCapacity;
    txCount++;
}

void CdcConsoleStream::update() {
    if (txCount == 0 || !tud_mounted()) return;

    size_t budget = txCount < maxDrainPerUpdate ? txCount : maxDrainPerUpdate;
    while (budget > 0) {
        uint32_t available = tud_cdc_n_write_available(0);
        if (available == 0) break;

        size_t contiguous = txCapacity - txTail;
        if (contiguous > txCount) contiguous = txCount;
        if (contiguous > budget) contiguous = budget;
        if (contiguous > available) contiguous = available;

        uint32_t sent = tud_cdc_n_write(0, txBuffer + txTail, contiguous);
        if (sent == 0) break;
        txTail = (txTail + sent) % txCapacity;
        txCount -= sent;
        budget -= sent;
    }
    tud_cdc_n_write_flush(0);
}
