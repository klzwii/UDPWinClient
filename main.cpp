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
#define BATCH_LENGTH 5
#define PACKET_SIZE 1000
#define RS_LENGTH 2

void RDT(int fd, const sockaddr * sendAddr);

std::atomic_bool started;
int main() {
    finished.store(false);
    started.store(false);
    int totalPackets = 0;
    auto  k = new RSHelper();
    FILE *fp;
    if (!(fp = fopen("D:\\chap1.pdf", "rb"))) {
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
    std::this_thread::sleep_for(std::chrono::seconds(30));
}
