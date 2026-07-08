#pragma once

#include <cstdint>

enum class ESPNowMessageHeader : uint8_t {
    Pairing = 0x00,
    PairingAck = 0x01,
    Connection = 0x02,
    Packet = 0x03,
    Info = 0x04,
};

struct ESPNowPairingMessage {
    ESPNowMessageHeader header = ESPNowMessageHeader::Pairing;
    uint8_t mac[6] = {0};
};

struct ESPNowPairingAckMessage {
    ESPNowMessageHeader header = ESPNowMessageHeader::PairingAck;
    uint8_t trackerId;
};

struct ESPNowConnectionMessage {
    ESPNowMessageHeader header = ESPNowMessageHeader::Connection;
    uint8_t trackerId;
};

struct ESPNowPacketMessage {
    ESPNowMessageHeader header = ESPNowMessageHeader::Packet;
    uint8_t data[15];
};

struct ESPNowInfoMessage {
    ESPNowMessageHeader header = ESPNowMessageHeader::Info;
    uint8_t data[14];
};

struct ESPNowMessageBase {
    ESPNowMessageHeader header;
};

union ESPNowMessage {
    ESPNowMessageBase base;
    ESPNowPairingMessage pairing;
    ESPNowPairingAckMessage pairingAck;
    ESPNowConnectionMessage connection;
    ESPNowPacketMessage packet;
    ESPNowInfoMessage info;
};
