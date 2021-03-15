//
// Created by liuze on 2021/3/14.
//

#ifndef UDPWINCLIENT_HEADERCONST_H
#define UDPWINCLIENT_HEADERCONST_H
#define HEADER_LENGTH 16
struct header {
private:
    static const uint8_t FIN = 0b1;
    static const uint8_t ACK = 0b10;
    static const uint8_t SYN = 0b100;
    static const uint8_t RST = 0b1000;
//    uint32_t crc;  // 4
//    uint16_t sendSeq; // 6
//    uint16_t ackSeq; // 8
//    uint16_t packetLength; // 10
//    uint8_t subSeq; // 11
//    uint8_t symbol; // 12
//    uint8_t realSeq; //13
//    uint8_t rsSeq; //14
//    uint16_t headStart; //15
    uint8_t *addr;
public:
    explicit header(uint8_t *bytes) {
        this->addr = bytes;
    }
    void Clear() {
        memset(addr, 0, HEADER_LENGTH);
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
    void SetRealSeq(uint8_t realSeq) {
        *reinterpret_cast<uint8_t*>(addr + 12) = realSeq;
    }
    void SetRSSeq(uint8_t rsSeq) {
        *reinterpret_cast<uint8_t*>(addr + 13) = rsSeq;
    }
    void SetHeadStart(uint16_t headStart) {
        *reinterpret_cast<uint16_t*>(addr + 14) = headStart;
    }
    void SetFIN(bool isSet) {
        if (isSet) {
            *reinterpret_cast<uint8_t*>(addr + 11) |= FIN;
        } else {
            *reinterpret_cast<uint8_t*>(addr + 11) &= ~FIN;
        }
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
    uint8_t RealSeq() {
        return *reinterpret_cast<uint8_t*>(addr + 12);
    }
    uint8_t RSSeq() {
        return *reinterpret_cast<uint8_t*>(addr + 13);
    }
    uint16_t HeadStart() {
        return *reinterpret_cast<uint16_t*>(addr + 14);
    }
    bool IsFin() {
        return *reinterpret_cast<uint8_t*>(addr + 11) & FIN;
    }
};

#endif //UDPWINCLIENT_HEADERCONST_H
