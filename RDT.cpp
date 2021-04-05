//
// Created by liuze on 2021/4/1.
//

#include "RDT.h"
#include <sys/time.h>
#include <crc32c/crc32c.h>
#include <thread>
#include <algorithm>
#include <linux/ip.h>

#ifdef server
int RDT::serverFD = 0;
#endif

long long operator - (const timeval &a, const timeval &b) {
    long long t = (a.tv_sec - b.tv_sec) * 1000 + (a.tv_usec - b.tv_usec) / 1000;
    if (t < 0) {
        t += 60 * 60 * 24 * 1000;
    }
    return t;
}

uint16_t RDT::calcCheckSum(uint16_t *data, size_t len, const uint16_t *fakeHead) {
    uint32_t cksum = 0;
    if (fakeHead != nullptr) {
        for (int i = 0; i < 6; i ++) {
            cksum += *(fakeHead + i);
        }
    }
    while(len > 1)
    {
        cksum += *(data++);
        len -= 2;
    }
    if (len == 1) {
        cksum += *(uint8_t*)data;
    }
    while (cksum >> 16) {
        uint16_t t = cksum >> 16;
        cksum &= 0x0000ffff;
        cksum += t;
    }
    return cksum;
}

void RDT::reCalcChecksum(uint16_t *payLoad, size_t len) {
    auto *iph = (iphdr*)payLoad;
    uint16_t headLen = iph->ihl * 4;
    std::cout << headLen << std::endl;
    uint8_t fakeHead[12];
    if (iph->protocol != IPPROTO_ICMP) {
        *(uint32_t*)fakeHead = iph->saddr;
        *(uint32_t*)(fakeHead + 4) = iph->daddr;
        *(uint8_t*)(fakeHead + 8) = 0;
        *(uint8_t*)(fakeHead + 9) = iph->protocol;
        *(uint16_t*)(fakeHead + 10) = htobe16((uint16_t)(len - headLen));
        if (iph->protocol == IPPROTO_TCP) {
            *(payLoad + headLen/2 + 8) = 0;
            *(payLoad + headLen/2 + 8) = ~calcCheckSum(payLoad + headLen/2, len - headLen, (uint16_t*)fakeHead);
        }
        if (iph->protocol == IPPROTO_UDP) {
            *(payLoad + headLen/2 + 3) = 0;
            *(payLoad + headLen/2 + 3) = ~calcCheckSum(payLoad + headLen/2, len - headLen, (uint16_t*)fakeHead);
        }
    }
    iph->check = 0;
    iph->check = ~(calcCheckSum(payLoad, len, nullptr));
}

// non thread safe
void RDT::SendBuffer() {
    uint16_t curSeq = SendWindowEnd;
    while ((uint16_t)(curSeq - SendWindowStart.load()) >= WINDOW_SIZE) {
        std::this_thread::yield();
    }
    uint8_t* p[20];
    ++SendWindowEnd;
    uint8_t* &currentBuffer = sendBuffers[SendWindowEnd % WINDOW_SIZE];
    for (int i = 0; i < RS_LENGTH; i++) {
        p[i] = currentBuffer + (i + BATCH_LENGTH) * SEGMENT_LENGTH + HEADER_LENGTH;
    }
    for (int i = 0; i < BATCH_LENGTH; i ++) {
        memcpy(currentBuffer + i * SEGMENT_LENGTH + HEADER_LENGTH, buffer + i * PACKET_SIZE, PACKET_SIZE);
        p[i + RS_LENGTH] = currentBuffer + i * SEGMENT_LENGTH + HEADER_LENGTH;
    }
    if (RS_LENGTH != 0) {
        helpers[0]->GenerateRSPacket(p, BATCH_LENGTH, RS_LENGTH, PACKET_SIZE);
    }
    for (int i = 0; i < RS_LENGTH + BATCH_LENGTH; i ++) {
        auto head = header(currentBuffer + i * SEGMENT_LENGTH);
        head.SetPacketLength(offset);
        head.SetRealSeq(BATCH_LENGTH);
        head.SetSendSeq(SendWindowEnd);
        head.SetRSSeq(RS_LENGTH);
        head.SetSubSeq(i);
        head.SetACK();
        head.SetHeadStart(firstHead);
        head.SetCRC(crc32c::Crc32c(currentBuffer + i * SEGMENT_LENGTH + 4, SEGMENT_LENGTH - 4));
    }
    sendBufferBySeq(SendWindowEnd);
}

void RDT::sendBufferBySeq(uint16_t seq) {
    std::cout << "send seq:" << seq << std::endl;
    uint8_t* &currentBuffer = sendBuffers[seq % WINDOW_SIZE];
    for (int i = 0; i < BATCH_LENGTH + RS_LENGTH; i ++) {
        auto head = header(currentBuffer + i * SEGMENT_LENGTH);
        if (head.AckSeq() != RecvStart) {
            head.SetAckSeq(RecvStart);
            head.SetCRC(crc32c::Crc32c(currentBuffer + i * SEGMENT_LENGTH + 4, SEGMENT_LENGTH - 4));
        }
        int sendRet = 0;
#ifdef client
        sendRet = send(sendFD, currentBuffer + i * SEGMENT_LENGTH, SEGMENT_LENGTH, 0);
#endif
#ifdef server
        sendRet = sendto(serverFD, currentBuffer + i * SEGMENT_LENGTH, SEGMENT_LENGTH, 0, sendSockAddr, *sockLen);
#endif
        if (sendRet < 0) {
            perror("send");
        }
    }
    gettimeofday(&sendTime[seq % WINDOW_SIZE], nullptr);
}

//non thread-safe
void RDT::RecvBuffer(uint8_t *data) {
    auto head = header(data);
    if(head.CRC() != crc32c::Crc32c(data + 4, SEGMENT_LENGTH - 4)) {
        return;
    }
    uint16_t diff = head.SendSeq() - RecvStart;
    if (diff > WINDOW_SIZE || diff == 0) {
        return;
    }
    diff = head.SendSeq() - RecvEnd;
    if (diff <= WINDOW_SIZE) {
        RecvEnd = head.SendSeq();
    }
    uint16_t pos = head.SendSeq() % WINDOW_SIZE;
    if (ack[pos]&(1 << head.SubSeq())) {
        return;
    }
    if (!ack[pos]) {
        PacketLength[pos] = head.PacketLength();
        FirstHeader[pos] = head.HeadStart();
        std::cout << "first head " << pos << " " << head.SendSeq() << " " << FirstHeader[pos] << std::endl;
    }
    ack[pos] |= (1 << head.SubSeq());
    gettimeofday(&recvTime[pos], nullptr);
    if (head.IsACK()) {
        diff = head.AckSeq() - SendWindowStart;
        if (diff != 0 && diff <= WINDOW_SIZE) {
            SendWindowStart.store(head.AckSeq());
        }
    }
    uint8_t* &currentBuffer = RecvBuffers[pos];
    memcpy(currentBuffer + head.SubSeq() * PACKET_SIZE, data + HEADER_LENGTH, PACKET_SIZE);
}

#ifdef client
void RDT::RecvThread(RDT* rdt) {
    std::cout << "recv thread started" << std::endl;
    fd_set fdSet{};
    timeval timeOut{0, 20000};
    FD_ZERO(&fdSet);
    auto sendFD = rdt->sendFD;
    FD_SET(sendFD, &fdSet);
    uint8_t buffer[rdt->SEGMENT_LENGTH];
    while (true) {
        auto setCopy = fdSet;
        auto tvCopy = timeOut;
        if (rdt->threadExit) {
            rdt->threadExit.store(false);
            return;
        }
        int selectedFd = select(sendFD + 1, &setCopy, nullptr, nullptr, &tvCopy);
        if (FD_ISSET(sendFD, &setCopy)) {
            //todo recv data
            auto recvRet = recvfrom(sendFD, buffer, rdt->SEGMENT_LENGTH, 0, nullptr, nullptr);
            if (recvRet < 0) {
                perror("receive from socket");
            }
            rdt->RecvBuffer(buffer);
            rdt->DumpData();
            rdt->TimeOut();
        } else if (selectedFd == 0) {
            rdt->TimeOut();
            rdt->DumpData();
        } else {
            perror("select");
        }
    }
}
#endif

void RDT::DumpDataBySeq(uint16_t curSeq, RDT* rdt, int id) {
    //std::cout << "curSeq: " << curSeq << std::endl;
    uint16_t pos = curSeq % rdt->WINDOW_SIZE;
    if (rdt->finish[pos]) {
        return;
    }
    const int &RS_LENGTH = rdt->RS_LENGTH;
    const int &BATCH_LENGTH = rdt->BATCH_LENGTH;
    const int &PACKET_SIZE = rdt->PACKET_SIZE;
    uint8_t ackBits = rdt->ack[pos];
    auto k = rdt->helpers[id];
    if (rdt->recvTime[pos].tv_usec == 0 && rdt->recvTime[pos].tv_sec == 0) {
        return;
    }
    uint8_t realBits = (1 << BATCH_LENGTH) - 1;
    bool canDump = false;
    if ((realBits & ackBits) == realBits) {
        canDump = true;
    } else {
        timeval curTime{};
        gettimeofday(&curTime, nullptr);
        auto diffTime = curTime - rdt->recvTime[pos];
        if (diffTime > rdt->RECOVER_THRESHOLD) {
            int nowPackets = 0;
            while (ackBits) {
                nowPackets += (1 & ackBits);
                ackBits >>= 1;
            }
            if ((RS_LENGTH >> 1) >= (RS_LENGTH + BATCH_LENGTH - nowPackets)) {
                uint8_t *p[RS_LENGTH + BATCH_LENGTH];
                uint8_t* &currentBuffer = rdt->RecvBuffers[pos];
                for (int i = 0; i < BATCH_LENGTH; i ++) {
                    p[i + RS_LENGTH] = currentBuffer + i * PACKET_SIZE;
                }
                for (int i = 0; i < RS_LENGTH; i ++) {
                    p[i] = currentBuffer + (BATCH_LENGTH + i) * PACKET_SIZE;
                }
                canDump = k->GetOriginMessageFromPackets(p, BATCH_LENGTH, RS_LENGTH, PACKET_SIZE);
                if (!canDump) {
                    printf("recover message fail seq:%d realLength:%d\n arrivePackets:%d ", curSeq, BATCH_LENGTH, nowPackets);
                }
            }
        }
    }
    if (canDump) {
        rdt->finish[pos] = true;
    }
}

void RDT::DumpDataThread(uint16_t seq, RDT* rdt, int id) {
    DumpDataBySeq(seq, rdt, id);
    int curWG = rdt->wg.load();
    while (!rdt->wg.compare_exchange_strong(curWG, curWG - 1)) {
        curWG =  rdt->wg.load();
    }
}

bool RDT::DumpData() {
    uint16_t start = RecvStart;
    wg.store(0);
    uint16_t end = RecvEnd;
    int id = 0;
    bool flg = false;
    for (uint16_t i = start + 1; end - i <= WINDOW_SIZE; ++i) {
        if (finish[i % WINDOW_SIZE]) {
            continue;
        }
        if (id >= THREAD_NUM) {
            break;
        }
        int curWG = wg.load();
        while (!wg.compare_exchange_strong(curWG, curWG + 1)) {
            curWG = wg.load();
        }
        std::thread(DumpDataThread, i, this, id).detach();
    }
    while (wg.load() != 0) {
        std::this_thread::yield();
    }
    for (uint16_t i = start + 1; end - i <= WINDOW_SIZE; ++i) {
        auto pos = i % WINDOW_SIZE;
        if (finish[pos]) {
            flg = true;
            uint16_t packetLength = PacketLength[pos];
            if (packetLength != 0) {
                uint16_t recvOffset = 0;
                uint16_t copyLength = FirstHeader[pos];
                memcpy(rawBuffer + rawOffset, RecvBuffers[pos], copyLength);
                rawOffset += copyLength;
                recvOffset += copyLength;
                SendRawBuffer();
                while (recvOffset != packetLength) {
                    uint16_t dataLeft = packetLength - recvOffset;
                    if (dataLeft >= sizeof(struct iphdr)) {
                        auto *hdr = reinterpret_cast<struct iphdr*>(RecvBuffers[pos] + recvOffset);
                        copyLength = be16toh(hdr->tot_len);
                    }
                    copyLength = std::min((uint16_t)(packetLength - recvOffset), copyLength);
                    memcpy(rawBuffer + rawOffset, RecvBuffers[pos] + recvOffset, copyLength);
                    rawOffset += copyLength;
                    recvOffset += copyLength;
                    SendRawBuffer();
                }
            }
            RecvStart = i;
            recvTime[pos] = timeval{0, 0};
            finish[pos] = false;
            ack[pos] = 0;
        } else {
            break;
        }
    }
    return flg;
}

void RDT::SendRawBuffer() {
    if (rawOffset < sizeof(struct iphdr)) {
        return;
    }
    auto *iphdr = reinterpret_cast<struct iphdr*>(rawBuffer);
    uint16_t sendLength = be16toh(iphdr->tot_len);
    if (sendLength != rawOffset) {
        return;
    }
    sockaddr_in tempSock{};
#ifdef client
    iphdr->daddr = fakeIP;
#endif
#ifdef server
    iphdr->saddr = fakeIP;
#endif
    tempSock.sin_family = AF_INET;
    tempSock.sin_addr.s_addr = iphdr->daddr;
    if (iphdr->protocol == IPPROTO_TCP) {
        tempSock.sin_port = *(uint16_t *) (rawBuffer + 4 * (iphdr->ihl) + 2);
    }
    std::cout << be16toh(tempSock.sin_port) << std::endl << be16toh(*(uint16_t *)(rawBuffer + 4 * (iphdr->ihl))) << std::endl;
    reCalcChecksum((uint16_t*)rawBuffer, sendLength);
    auto sendRet = sendto(rawSocket, rawBuffer, sendLength, 0, (sockaddr*)&tempSock, sizeof(sockaddr_in));
    std::cout << "send raw " << sendLength << (int)iphdr->ihl << std::endl;
    if (sendRet < 0) {
        perror("send raw");
    }
    rawOffset = 0;
}

void RDT::AddData(uint8_t *data, size_t length, bool initial) {
    size_t copyLength = std::min(DATA_LENGTH - offset, length);
    memcpy(this->buffer + offset, data, copyLength);
    if (offset == 0) {
        gettimeofday(&bufferStartTime, nullptr);
    }
    if (initial) {
        if (firstHead > offset) {
            firstHead = offset;
        }
    }
    offset += copyLength;
    length -= copyLength;
    data += copyLength;
    if (offset == DATA_LENGTH) {
        SendBuffer();
        offset = 0;
        firstHead = DATA_LENGTH;
    }
    if (length > 0) {
        AddData(data, length, false);
    }
}


// non thread-safe should be called in send thread;
void RDT::BufferTimeOut() {
    if (!offset) {
        return;
    }
    timeval curTime{};
    gettimeofday(&curTime, nullptr);
    if (curTime - bufferStartTime > BUFFER_THRESHOLD) {
        std::cout << "buffer time out" << std::endl;
        if (firstHead > offset) {
            firstHead = offset;
        }
        SendBuffer();
        offset = 0;
        firstHead = DATA_LENGTH;
    }
}


// must not modify SendWindowStart during this func, should be called in recv thread
void RDT::TimeOut() {
    timeval curTime{};
    gettimeofday(&curTime, nullptr);
    for (uint16_t i = SendWindowStart + 1; SendWindowEnd - i <= WINDOW_SIZE; i ++) {
        if (curTime - sendTime[i % WINDOW_SIZE] > RESEND_THRESHOLD) {
            sendBufferBySeq(i);
        } else {
            break;
        }
    }
}

#ifdef server
void RDT::init(int fd) {
    serverFD = fd;
}
#endif

