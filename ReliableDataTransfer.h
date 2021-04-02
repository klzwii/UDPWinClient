//
// Created by liuze on 2021/3/15.
//

#ifndef UDPWINCLIENT_RELIABLEDATATRANSFER_H
#define UDPWINCLIENT_RELIABLEDATATRANSFER_H
#include <atomic>
#include <algorithm>
#include <thread>
#include <sys/time.h>
#include "RSHelper.h"
#define WINDOW_SIZE 128
#define BATCH_LENGTH 5
#define PACKET_SIZE 1000
#define RS_LENGTH 4
#define TRY_RECOVER_THRESHOLD 2
#define THREAD_NUM 8
#define RESEND_THRESHOLD 700
#define PACKET_LOSS 10



static std::atomic_uint16_t recvEnd;
static std::atomic_uint16_t recvStart;
static std::atomic_uint16_t SendWindowEnd;
static std::atomic_bool finished;
static std::atomic_uint16_t SendWindowStart;
static uint8_t **buffers;
static uint8_t **sendBuffers;
static std::atomic_uint16_t ack[WINDOW_SIZE];
static std::atomic_uint8_t realLength[WINDOW_SIZE];
static std::atomic_uint16_t packetLengths[WINDOW_SIZE];
static bool finish[WINDOW_SIZE];
static std::atomic_uint16_t wg;
static timeval recvTime[WINDOW_SIZE];
static RSHelper* helpers[WINDOW_SIZE];
static int UDPSock;
static std::atomic_bool closed;
static struct sockaddr_in sSendAddr{};
static timeval sendTime[WINDOW_SIZE];
FILE *file;

void RDT();

void init() {
    for (int i = 0; i < THREAD_NUM; i ++) {
        helpers[i] = new RSHelper();
    }
    closed.store(false);
    memset(ack, 0, sizeof(ack));
    memset(finish, 0, sizeof(finish));
    memset(sendTime, 0, sizeof(sendTime));
    if ((UDPSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("open socket");
        exit(0);
    }
    sSendAddr.sin_family = AF_INET;
    sSendAddr.sin_addr.s_addr = inet_addr("159.75.92.163");
    sSendAddr.sin_port = htons(1234);
    std::thread(RDT).detach();
    buffers = reinterpret_cast<uint8_t**>(calloc(sizeof(uint8_t **), WINDOW_SIZE));
    finished.store(false);
    for (int i = 0; i < WINDOW_SIZE; i ++) {
        buffers[i] = reinterpret_cast<uint8_t*>(calloc(10000, 1));
        ack[i].store(0);
    }
    sendBuffers = reinterpret_cast<uint8_t**>(calloc(sizeof(uint8_t **), WINDOW_SIZE));
    for (int i = 0; i < WINDOW_SIZE; i ++) {
        sendBuffers[i] = reinterpret_cast<uint8_t*>(calloc(10000, 1));
        ack[i].store(0);
    }
}

void negotiateConfig() {
    SendWindowStart.store(0);
    SendWindowEnd.store(0);
    init();
    //todo negotiate between server and client
}

void close() {
    fflush(file);
    fclose(file);
}

void setData(uint16_t seq, uint8_t subSeq, uint8_t realSeq, uint8_t* buffer, uint16_t packetLength) {
    auto curSec = SendWindowStart.load();
    if ((seq - curSec) >= WINDOW_SIZE) {
        return;
    }
    const auto pos = seq % WINDOW_SIZE;
    memcpy(buffers[pos] + subSeq *  PACKET_SIZE, buffer + HEADER_LENGTH, PACKET_SIZE);
    auto befAck = ack[pos].load();
    uint8_t aftAck = befAck | (1<<subSeq);
    while (!ack[pos].compare_exchange_strong(befAck, aftAck)) {
        befAck = ack[pos].load();
        aftAck = befAck | (1<<subSeq);
        //std::cout << aftAck << std::endl;
    }
    uint8_t realBits = (1 << realSeq) - 1;
    if ((realBits & aftAck) == realBits) {
        finish[pos].store(true);
        curSec = SendWindowStart.load();
        if (curSec == seq) {
            for (uint8_t i = 0; i < WINDOW_SIZE; i ++) {
                auto tempPos = (pos + i) % WINDOW_SIZE;
                if (finish[tempPos].load()) {
                    uint16_t befSeq = curSec + i;
                    if (!SendWindowStart.compare_exchange_strong(befSeq, curSec + i + 1)) {
                            break;
                    }
                    fwrite(buffers[tempPos], 1, packetLength, file);
                    finish[tempPos].store(false);
                    ack[tempPos].store(0);
                } else {
                    break;
                }
            }
        }
    }
}

uint8_t* GetBuffer(uint16_t seq) {
    if (finished.load()) {
        printf("call get buffer after sendFin");
        exit(-1);
    }
    while (SendWindowEnd - SendWindowStart.load() >= WINDOW_SIZE) {
        std::this_thread::yield();
    }
    if (!SendWindowEnd.compare_exchange_strong(seq, seq + 1)) {
        return nullptr;
    }
    return sendBuffers[seq % WINDOW_SIZE];
}

void sendBuffer(const uint16_t &seqNumber) {
    auto pos = seqNumber % WINDOW_SIZE;
    const auto &buffer = sendBuffers[pos];
    auto head = header(buffer);
    uint16_t sendSize = PACKET_SIZE + HEADER_LENGTH;
    if (head.IsFin()) {
        sendSize = HEADER_LENGTH;
    }
    uint8_t totalLength = head.RSSeq() + head.RealSeq();
    for (int i = 0; i < totalLength; i ++) {
        auto t = rand() % 100;
        if (t < PACKET_LOSS) {
            continue;
        }
        sendto(UDPSock, buffer + i * sendSize, sendSize, 0,
               reinterpret_cast<const sockaddr *>(&sSendAddr), sizeof(sockaddr_in));
    }
    gettimeofday(&sendTime[pos], nullptr);
}

void SendFin() {
    auto seqNumber = SendWindowEnd.load();
    uint8_t *buffer;
    while ((buffer = GetBuffer(seqNumber)) == nullptr) {
        seqNumber = SendWindowEnd.load();
    }
    //std::cout << "send fin " << seqNumber << std::endl;
    finished.store(true);
    auto head = header(buffer);
    head.Clear();
    head.SetFIN();
    head.SetSendSeq(seqNumber);
    head.SetRealSeq(1);
    //std::cout << (int)head.RSSeq() << " " << (int)head.RealSeq();
    sendBuffer(seqNumber);
}

void DealBuffer(const uint16_t &seqNumber, RSHelper *k, const uint16_t &packetLength, const uint8_t &realLength) {
    auto buffer = sendBuffers[seqNumber % WINDOW_SIZE];
    uint8_t* p[10];
    auto totalLength = PACKET_SIZE + HEADER_LENGTH;
    for (int i = 0; i < RS_LENGTH; i ++) {
        p[i] = buffer + (realLength + i) * totalLength + HEADER_LENGTH;
    }
    for (int i = 0; i < realLength; i ++) {
        p[i + RS_LENGTH] = buffer + i * totalLength + HEADER_LENGTH;
    }
    k->GenerateRSPacket(p, realLength, RS_LENGTH, PACKET_SIZE);
    for (int i = 0; i < realLength + RS_LENGTH; i ++) {
        auto startAddr = buffer + i * totalLength;
        header head = header(startAddr);
        head.Clear();
        head.SetRealSeq(realLength);
        head.SetRSSeq(RS_LENGTH);
        head.SetSendSeq(seqNumber);
        head.SetPacketLength(packetLength);
        head.SetSubSeq(i);
        head.SetCRC(crc32c::Crc32c(startAddr + 4, totalLength - 4));
    }
    sendBuffer(seqNumber);
}

void RDT() {
    struct sockaddr_in  clientADDR{};
    auto sendAddr = reinterpret_cast<const sockaddr *>(&sSendAddr);
    socklen_t clientADDRLen = sizeof(clientADDR);
    uint8_t buffer[2000];
    auto head = header(buffer);
    uint16_t length;
    fd_set FDSet;
    FD_ZERO(&FDSet);
    FD_SET(UDPSock, &FDSet);
    timeval tv{0, 200000};
    while (true) {
        auto FDSetCopy = FDSet;
        auto TVCopy = tv;
        auto selectFD = select(UDPSock + 1, &FDSetCopy, nullptr, nullptr, &TVCopy);
        if (selectFD < 0) {
            perror("socket");
            exit(-1);
        } else if (FD_ISSET(UDPSock, &FDSetCopy)) {
            recvfrom(UDPSock, buffer, 10000, 0, reinterpret_cast<sockaddr *>(&clientADDR), &clientADDRLen);
            if (head.IsFin()) {
                std::cout << "client end" << std::endl;
                closed.store(true);
                break;
            }
            if (head.IsACK()) {
                auto nowAck = SendWindowStart.load();
                auto ackSeq = head.AckSeq() + 1;
                while ((ackSeq - nowAck) <= WINDOW_SIZE && !SendWindowStart.compare_exchange_strong(nowAck, ackSeq)) {
                    nowAck = SendWindowStart.load();
                }
            }
        }
        uint16_t winStart = SendWindowStart.load(), winEnd = SendWindowEnd.load();
        timeval curTime{};
        gettimeofday(&curTime, nullptr);
        for (uint16_t k = winStart; k != winEnd; k ++) {
            auto diff = (curTime.tv_sec - sendTime[k % WINDOW_SIZE].tv_sec) * 1000 + (curTime.tv_usec - sendTime[k % WINDOW_SIZE].tv_usec) / 1000;
            if (winStart != winEnd && diff > RESEND_THRESHOLD) {
                sendBuffer(winStart);
            } else {
                break;
            }
        }
    }
}



#endif //UDPWINCLIENT_RELIABLEDATATRANSFER_H
