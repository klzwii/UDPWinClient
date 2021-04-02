//
// Created by liuze on 2021/4/1.
//

#ifndef UDPWINCLIENT_RDT_H
#define UDPWINCLIENT_RDT_H
#include <stdint.h>
#include <atomic>
#include <malloc.h>
#include <cstring>
#include "HeaderConst.h"
#include "RSHelper.h"
#include <netinet/in.h>

class RDT {
private:
    uint16_t RecvEnd;
    uint16_t RecvStart;
    std::atomic_uint16_t SendWindowStart; // recv thread changes this variable and send thread also read this variable
    uint32_t *ack;
    bool *finish;
    uint8_t **RecvBuffers;
    uint16_t SendWindowEnd;
    uint8_t **sendBuffers;
    RSHelper **helpers;
    timeval *sendTime;
    timeval *recvTime;
    int sendFD;
    uint8_t * buffer;
    timeval bufferStartTime;
    size_t offset;
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
    uint8_t *emptyBuffer;
public:
    void sendBufferBySeq(uint16_t seq);
    RDT(uint WINDOW_SIZE, uint BATCH_LENGTH, uint PACKET_SIZE,uint RS_LENGTH, in_addr_t SendAddr, uint32_t RECOVER_THRESHOLD, uint32_t RESEND_THRESHOLD, uint32_t BUFFER_THRESHOLD) {
        if ((WINDOW_SIZE & -WINDOW_SIZE) != WINDOW_SIZE) {
            printf("WINDOW_SIZE should be integer power of 2\n");
            exit(-1);
        }
        this->WINDOW_SIZE = WINDOW_SIZE;
        this->BATCH_LENGTH = BATCH_LENGTH;
        this->PACKET_SIZE = PACKET_SIZE;
        this->RECOVER_THRESHOLD = RECOVER_THRESHOLD;
        this->RESEND_THRESHOLD = RESEND_THRESHOLD;
        this->BUFFER_THRESHOLD = BUFFER_THRESHOLD;
        SEGMENT_LENGTH = PACKET_SIZE + HEADER_LENGTH;
        this->RS_LENGTH = RS_LENGTH;
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
        struct sockaddr_in sockAddr{};
        sockAddr.sin_addr.s_addr = SendAddr;
        sockAddr.sin_family = AF_INET;
        sendFD = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sendFD < 0) {
            perror("open socket");
            exit(-1);
        }
        if (connect(sendFD, (sockaddr*)&sockAddr, sizeof(sockAddr)) < 0) {
            perror("connect");
            exit(-1);
        }
        sendTime = reinterpret_cast<timeval*>(malloc(sizeof(timeval) * WINDOW_SIZE));
        recvTime = reinterpret_cast<timeval*>(malloc(sizeof(timeval) * WINDOW_SIZE));
        finish = reinterpret_cast<bool*>(calloc(WINDOW_SIZE, sizeof(bool)));
        buffer = reinterpret_cast<uint8_t*>(calloc(PACKET_SIZE, sizeof(uint8_t)));
        emptyBuffer = reinterpret_cast<uint8_t*>(calloc(PACKET_SIZE, sizeof(uint8_t)));
    };
    void AddData(uint8_t *buffer, size_t length);
    void SendBuffer(uint8_t *buffer);
    void RecvBuffer(uint8_t *buffer);
    void DumpData();

    static void DumpDataBySeq(uint16_t curSeq, RDT* rdt, int id);

    static void DumpDataThread(uint16_t seq, RDT* rdt, int id);

    void TimeOut();

    void BufferTimeOut();

    static void RecvThread(RDT *rdt);
};


#endif //UDPWINCLIENT_RDT_H
