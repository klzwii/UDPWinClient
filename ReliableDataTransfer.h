//
// Created by liuze on 2021/3/15.
//

#ifndef UDPWINCLIENT_RELIABLEDATATRANSFER_H
#define UDPWINCLIENT_RELIABLEDATATRANSFER_H
#include <atomic>
#include <mutex>
#define windowSize 10

static std::atomic_uint32_t currentSeq;
static std::atomic_uint32_t ackSeq;
static uint8_t **buffers;
static std::once_flag t;
static std::atomic_uint8_t ack[windowSize];

void init() {
    buffers = reinterpret_cast<uint8_t**>(sizeof(uint8_t **) * windowSize);
    for (int i = 0; i < windowSize; i ++) {
        buffers[i] = reinterpret_cast<uint8_t*>(10000);
        ack[i].store(0);
    }
}

void negotiateConfig() {
    currentSeq.store(0);
    ackSeq.store(0);
    std::call_once(t, init);
    //todo negotiate between server and client
}

#endif //UDPWINCLIENT_RELIABLEDATATRANSFER_H
