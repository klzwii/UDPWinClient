//
// Created by liuze on 2021/4/1.
//

#include "RDT.h"
#include <sys/time.h>
#include <crc32c/crc32c.h>
#include <thread>
#include <algorithm>

long operator - (const timeval &a, const timeval &b) {
    auto t = (a.tv_sec - b.tv_sec) * 1000 + (a.tv_usec - b.tv_usec) / 1000;
    if (t < 0) {
        t += 60 * 60 * 24 * 1000;
    }
    return t;
}

// non thread safe
void RDT::SendBuffer(uint8_t *buffer) {
    uint16_t curSeq = SendWindowEnd;
    while ((uint16_t)(curSeq - SendWindowStart.load()) > WINDOW_SIZE) {
    }
    uint8_t* p[20];
    uint8_t* &currentBuffer = sendBuffers[SendWindowEnd % WINDOW_SIZE];
    for (int i = 0; i < RS_LENGTH; i++) {
        p[i] = currentBuffer + (i + BATCH_LENGTH) * SEGMENT_LENGTH + HEADER_LENGTH;
    }
    for (int i = 0; i < BATCH_LENGTH; i ++) {
        p[i + RS_LENGTH] = buffer + i * PACKET_SIZE;
        memcpy(currentBuffer + i * SEGMENT_LENGTH + HEADER_LENGTH, buffer + i * PACKET_SIZE, PACKET_SIZE);
    }
    helpers[0]->GenerateRSPacket(p, BATCH_LENGTH, RS_LENGTH, PACKET_SIZE);
    for (int i = 0; i < RS_LENGTH + BATCH_LENGTH; i ++) {
        auto head = header(currentBuffer + i * SEGMENT_LENGTH);
        head.SetCRC(crc32c::Crc32c(currentBuffer + i * SEGMENT_LENGTH + 4, 12 + PACKET_SIZE));
        head.SetPacketLength(PACKET_SIZE);
        head.SetRealSeq(BATCH_LENGTH);
        head.SetSendSeq(curSeq);
        head.SetRSSeq(RS_LENGTH);
        head.SetSubSeq(i);
    }
    sendBufferBySeq(SendWindowEnd++);
}

void RDT::sendBufferBySeq(uint16_t seq) {
    uint8_t* &currentBuffer = sendBuffers[seq % WINDOW_SIZE];
    for (int i = 0; i < BATCH_LENGTH + RS_LENGTH; i ++) {
        if (send(sendFD, currentBuffer + i * SEGMENT_LENGTH, SEGMENT_LENGTH, 0) <  0) {
            perror("send");
        }
    }
    gettimeofday(&sendTime[seq % WINDOW_SIZE], nullptr);
}

//non thread-safe
void RDT::RecvBuffer(uint8_t *buffer) {
    auto head = header(buffer);
    if(head.CRC() != crc32c::Crc32c(buffer + 4, HEADER_LENGTH - 4 + PACKET_SIZE)) {
        return;
    }
    if (head.SendSeq() - RecvStart > WINDOW_SIZE) {
        return;
    }
    if (head.SendSeq() > RecvEnd) {
        RecvEnd = head.SendSeq();
    }
    uint16_t pos = head.SendSeq() % WINDOW_SIZE;
    gettimeofday(&recvTime[pos], nullptr);
    if (!(ack[pos]&(1 << head.SubSeq()))) {
        return;
    }
    ack[pos] &= (1 << head.SubSeq());
    uint16_t curSendWindowStart = SendWindowStart.load();
    if (head.IsACK()) {
        if (head.AckSeq() - curSendWindowStart > 0 && head.AckSeq() - curSendWindowStart <= WINDOW_SIZE) {
            SendWindowStart.store(head.AckSeq());
        }
    }
    uint8_t* &currentBuffer = RecvBuffers[pos];
    memcpy(currentBuffer + head.SubSeq() * PACKET_SIZE, buffer, SEGMENT_LENGTH);
}

void RDT::RecvThread(RDT* rdt) {
    fd_set fdSet{};
    timeval timeOut{0, 200000};
    FD_ZERO(&fdSet);
    auto sendFD = rdt->sendFD;
    FD_SET(sendFD, &fdSet);
    uint8_t buffer[rdt->PACKET_SIZE];
    while (true) {
        auto setCopy = fdSet;
        auto tvCopy = timeOut;
        int selectedFd = select(sendFD + 1, &setCopy, nullptr, nullptr, &tvCopy);
        if (FD_ISSET(sendFD, &setCopy)) {
            //todo recv data
            auto recvRet = recvfrom(sendFD, buffer, rdt->PACKET_SIZE, 0, nullptr, nullptr);
            if (recvRet < 0) {
                perror("receive from socket");
            }
            rdt->RecvBuffer(buffer);
        } else if (selectedFd == 0) {

        } else {
            perror("select");
        }
    }
}

void RDT::DumpDataBySeq(uint16_t curSeq, RDT* rdt, int id) {
    ++curSeq;
    std::cout << "curSeq: " << curSeq << std::endl;
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

void RDT::DumpData() {
    uint16_t start = RecvStart;
    wg.store(0);
    uint16_t end = RecvEnd;
    int id = 0;
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
            //todo send by raw sockets
            RecvStart = i;
            recvTime[pos] = timeval{0, 0};
            finish[pos] = false;
            ack[pos] = 0;
        } else {
            break;
        }
    }
}

void RDT::AddData(uint8_t *buffer, size_t length) {
    BufferTimeOut();
    int curPos = SendWindowEnd & 1;
    size_t copyLength = std::min(PACKET_SIZE - offset, length);
    memcpy(this->buffer + offset, buffer, copyLength);
    if (offset == 0) {
        gettimeofday(&bufferStartTime, nullptr);
    }
    offset += copyLength;
    length -= copyLength;
    buffer += copyLength;
    if (offset == PACKET_SIZE) {
        SendBuffer(this->buffer);
        offset = 0;
    }
    if (length > 0) {
        AddData(buffer, length);
    }
}


// non thread-safe should be put in SendThread;
void RDT::BufferTimeOut() {
    timeval curTime{};
    gettimeofday(&curTime, nullptr);
    if (curTime - bufferStartTime > BUFFER_THRESHOLD) {
        AddData(emptyBuffer, PACKET_SIZE - offset);
    }
}


// must not modify SendWindowStart during process, should be called in recv thread
void RDT::TimeOut() {
    timeval curTime{};
    gettimeofday(&curTime, nullptr);
    for (uint16_t i = SendWindowStart + 1; SendWindowEnd - i <= WINDOW_SIZE; i ++) {
        if (sendTime[i] - curTime > RESEND_THRESHOLD) {
            sendBufferBySeq(i);
        } else {
            break;
        }
    }
}

