#include <ctime>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "HeaderConst.h"
#include "RSHelper.h"
#include "crc32c/crc32c.h"
#include "ReliableDataTransfer.h"
#include <thread>
#include <atomic>
#include <chrono>

std::atomic_bool started;
int main() {
    srand(time(nullptr));
    finished.store(false);
    started.store(false);
    int totalPackets = 0;
    auto  k = new RSHelper();
    FILE *fp;
    if (!(fp = fopen("/mnt/d/chap1.pdf", "rb"))) {
        perror("open file");
    }
    auto start = time(nullptr);
    auto totalLength = PACKET_SIZE + HEADER_LENGTH;
    uint8_t* p[10];
    negotiateConfig();
    while (true) {
        if (ferror(fp) || feof(fp)) {
            break;
        }
        uint8_t subSeq;
        uint16_t length;
        uint16_t packetLength = 0;
        uint8_t* buffer;
        uint32_t seqNumber = SendWindowEnd;
        while ((buffer = GetBuffer(seqNumber)) == nullptr) {
            seqNumber = SendWindowEnd.load();
        }
        for (subSeq = 0; subSeq < BATCH_LENGTH; subSeq ++) {
            auto startAddr = buffer + subSeq * totalLength;
            length = fread(startAddr + HEADER_LENGTH, 1, PACKET_SIZE, fp);
            packetLength += length;
            if (feof(fp)) {
                ++subSeq;
                memset(startAddr + length, 0, sizeof(startAddr));
                break;
            }
            if (ferror(fp)) {
                break;
            }
        }
        if (ferror(fp)) {
            perror("read file");
            exit(0);
        }
        DealBuffer(seqNumber, k, packetLength, subSeq);
    }
    SendFin();
    auto end =time(nullptr);
    printf("time=%f\n total:%d\n", difftime(end,start), totalPackets);
    if (ferror(fp)) {
        perror("read file");
    }
    while (!closed.load()) {
        std::this_thread::yield();
    }
}
