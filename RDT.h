//
// Created by liuze on 2021/4/1.
//

#ifndef UDPCOMMON_RDT_H
#define UDPCOMMON_RDT_H
#define client
#include <stdint.h>
#include <atomic>
#include <malloc.h>
#include <cstring>
#include "HeaderConst.h"
#include "RSHelper.h"
#include <thread>
#include <random>
#include <netinet/in.h>

class RDT {
private:
    uint16_t RecvEnd;
    uint16_t RecvStart;
    std::atomic_uint16_t SendWindowStart; // recv thread changes this variable and send thread also read this variable
    uint32_t *ack;
    bool *finish;
#ifdef server
    static int serverFD; // for server only
    sockaddr *sendSockAddr;
    socklen_t *sockLen;
#endif
    uint8_t **RecvBuffers;
    uint16_t SendWindowEnd;
    uint16_t uuid;
    uint8_t **sendBuffers;
    uint8_t rawBuffer[2000];
    uint16_t rawOffset = 0;
    RSHelper **helpers;
    timeval *sendTime;
    timeval *recvTime;
#ifdef client
    int sendFD;
    static void RecvThread(RDT *rdt);
#endif
    uint8_t * buffer;
    timeval bufferStartTime{0,0};
    size_t offset = 0;
    uint THREAD_NUM;
    uint WINDOW_SIZE;
    uint BATCH_LENGTH;
    uint PACKET_SIZE;
    uint RS_LENGTH;
    uint SEGMENT_LENGTH;
    uint32_t RESEND_THRESHOLD;
    uint32_t RECOVER_THRESHOLD;
    uint32_t BUFFER_THRESHOLD;
    std::atomic_int wg;
    in_addr_t fakeIP;
    int rawSocket;
    uint16_t *PacketLength;
    static void reCalcChecksum(uint16_t *payLoad, size_t len);
public:
    void sendBufferBySeq(uint16_t seq);
#ifdef server
    static void init(int fd);
#endif
#ifdef client
    RDT(uint WINDOW_SIZE, uint BATCH_LENGTH, uint PACKET_SIZE, uint RS_LENGTH, in_addr_t SendAddr, in_port_t SendPort, uint32_t RECOVER_THRESHOLD, uint32_t RESEND_THRESHOLD, uint32_t BUFFER_THRESHOLD, in_addr_t fakeIP)
#endif
#ifdef server
    RDT(uint WINDOW_SIZE, uint BATCH_LENGTH, uint PACKET_SIZE, uint RS_LENGTH, sockaddr* sockADDR, socklen_t* sockLen, uint32_t RECOVER_THRESHOLD, uint32_t RESEND_THRESHOLD, uint32_t BUFFER_THRESHOLD, in_addr_t fakeIP)
#endif
    {
        if ((WINDOW_SIZE & -WINDOW_SIZE) != WINDOW_SIZE) {
            printf("WINDOW_SIZE should be integer power of 2\n");
            exit(-1);
        }
#ifdef server
        if (serverFD <= 0) {
            printf("should first initialize server socket fd\n");
            exit(-1);
        }
        sendSockAddr = sockADDR;
        this->sockLen = sockLen;
#endif
        SendWindowStart = -1;
        SendWindowEnd = -1;
        RecvStart = -1;
        RecvEnd = -1;
        this->WINDOW_SIZE = WINDOW_SIZE;
        this->BATCH_LENGTH = BATCH_LENGTH;
        this->PACKET_SIZE = PACKET_SIZE;
        this->RECOVER_THRESHOLD = RECOVER_THRESHOLD;
        this->RESEND_THRESHOLD = RESEND_THRESHOLD;
        this->BUFFER_THRESHOLD = BUFFER_THRESHOLD;
        SEGMENT_LENGTH = PACKET_SIZE + HEADER_LENGTH;
        this->RS_LENGTH = RS_LENGTH;
        this->THREAD_NUM = 4;
        uint bufferSize = SEGMENT_LENGTH * (RS_LENGTH + BATCH_LENGTH);
        sendBuffers = reinterpret_cast<uint8_t**>(calloc(sizeof(void *), WINDOW_SIZE));
        for (int i = 0; i < WINDOW_SIZE; i ++) {
            sendBuffers[i] = reinterpret_cast<uint8_t*>(calloc(bufferSize, 1));
        }
        RecvBuffers = reinterpret_cast<uint8_t**>(calloc(sizeof(void *), WINDOW_SIZE));
        for (int i = 0; i < WINDOW_SIZE; i ++) {
            RecvBuffers[i] = reinterpret_cast<uint8_t*>(calloc(bufferSize, 1));
        }
        ack = reinterpret_cast<uint32_t*>(calloc(sizeof(uint32_t), WINDOW_SIZE));
        SEGMENT_LENGTH = PACKET_SIZE + HEADER_LENGTH;
        helpers = reinterpret_cast<RSHelper**>(malloc(sizeof(void *) * THREAD_NUM));
        for (int i = 0; i < THREAD_NUM; i ++) {
            helpers[i] = new RSHelper();
        }
        sendTime = reinterpret_cast<timeval*>(malloc(sizeof(timeval) * WINDOW_SIZE));
        recvTime = reinterpret_cast<timeval*>(malloc(sizeof(timeval) * WINDOW_SIZE));
        finish = reinterpret_cast<bool*>(calloc(WINDOW_SIZE, sizeof(bool)));
        buffer = reinterpret_cast<uint8_t*>(calloc(PACKET_SIZE, sizeof(uint8_t)));
        PacketLength = reinterpret_cast<uint16_t*>(calloc(WINDOW_SIZE, sizeof(uint16_t)));
        rawSocket = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
        if (rawSocket < 0) {
            perror("open raw socket");
        }
        this->fakeIP = fakeIP;
#ifdef client
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::mt19937 rand_num(seed);
        std::uniform_int_distribution<uint16_t> dist;
        uuid = dist(rand_num);
        struct sockaddr_in sockAddr{};
        sockAddr.sin_addr.s_addr = SendAddr;
        sockAddr.sin_family = AF_INET;
        sockAddr.sin_port = SendPort;
        sendFD = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sendFD < 0) {
            perror("open socket");
            exit(-1);
        }
        if (connect(sendFD, (sockaddr*)&sockAddr, sizeof(sockAddr)) < 0) {
            perror("connect");
            exit(-1);
        }
        std::thread(RecvThread, this).detach();
#endif
    };

    void AddData(uint8_t *buffer, size_t length);

    void RecvBuffer(uint8_t *data);

    bool DumpData();

    static void DumpDataBySeq(uint16_t curSeq, RDT* rdt, int id);

    static void DumpDataThread(uint16_t seq, RDT* rdt, int id);

    void TimeOut();

    void BufferTimeOut();

    void SendBuffer(uint8_t *data, uint16_t length);
};


#endif
