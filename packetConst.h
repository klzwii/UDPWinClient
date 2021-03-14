//
// Created by liuze on 2021/3/14.
//

#ifndef UDPWINCLIENT_PACKETCONST_H
#define UDPWINCLIENT_PACKETCONST_H
#include <sys/_stdint.h>
#define HEADER_LENGTH 12
struct header {
private:
    uint32_t crc;  // 4
    uint16_t sendSeq; // 6
    uint16_t ackSeq; // 8
    uint16_t packetLength; // 10
    uint8_t subSeq; // 11
    uint8_t allSeq; // 12
    uint8_t *addr;
public:
    header(uint8_t *bytes, bool exist) {
        if (exist) {
            this->crc = *reinterpret_cast<uint32_t*>(bytes);
            this->sendSeq = *reinterpret_cast<uint16_t*>(bytes + 4);
            this->ackSeq = *reinterpret_cast<uint16_t*>(bytes + 6);
            this->packetLength = *reinterpret_cast<uint16_t*>(bytes + 8);
            this->subSeq = *reinterpret_cast<uint8_t*>(bytes + 10);
            this->allSeq = *reinterpret_cast<uint8_t*>(bytes + 11);
        }
        this->addr = bytes;
    }
    void SetCRC(uint32_t crc) {
        *reinterpret_cast<uint32_t*>(addr) = crc;
    }
    void SetSendSeq(uint16_t sendSeq) {
        *reinterpret_cast<uint16_t*>(addr + 4) = sendSeq;
    }
    void SetAckSeq(uint16_t ackSeq) {
        *reinterpret_cast<uint16_t*>(addr + 6) = ackSeq;
    }
    void SetPacketLength(uint16_t packetLength) {
        *reinterpret_cast<uint16_t*>(addr + 8) = packetLength;
    }
    void SetSubSeq(uint8_t subSeq) {
        *reinterpret_cast<uint8_t*>(addr + 10) = subSeq;
    }
    void SetAllSeq(uint8_t allSeq) {
        *reinterpret_cast<uint8_t*>(addr + 11) = allSeq;
    }
    uint32_t CRC() {
        return *reinterpret_cast<uint32_t*>(addr);
    }
    uint16_t SendSeq() {
        return *reinterpret_cast<uint16_t*>(addr + 4);
    }
    uint16_t AckSeq() {
        return *reinterpret_cast<uint16_t*>(addr + 6);
    }
    uint16_t PacketLength() {
        return *reinterpret_cast<uint16_t*>(addr + 8);
    }
    uint8_t SubSeq() {
        return *reinterpret_cast<uint8_t*>(addr + 10);
    }
    uint8_t AllSeq() {
        return *reinterpret_cast<uint8_t*>(addr + 11);
    }
};

#endif //UDPWINCLIENT_PACKETCONST_H
