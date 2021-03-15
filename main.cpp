#include <ctime>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "HeaderConst.h"
#include "RSHelper.h"
#include "crc32c/crc32c.h"
#include <thread>
#include <atomic>
#include <chrono>
#define batchLength 5
#define packetSize 1000
#define rsLength 2

uint32_t seqNumber = 0;
void RDT(int fd, const sockaddr * sendAddr);

std::atomic_bool finished;
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
    FILE  *test = fopen("D:\\12est.pdf", "wb");
    uint8_t buffer[10000];
    int UDPSock;
    if ((UDPSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("open socket");
    }

    struct sockaddr_in sSendAddr{};
    sSendAddr.sin_family = AF_INET;
    sSendAddr.sin_addr.s_addr = inet_addr("159.75.92.163");
    sSendAddr.sin_port = htons(1234);
    std::thread(RDT, UDPSock, reinterpret_cast<const sockaddr *>(&sSendAddr)).detach();
    auto start = time(nullptr);
    auto totalLength = packetSize + HEADER_LENGTH;
    uint8_t* p[10];
    while (true) {
        if (ferror(fp) || feof(fp)) {
            break;
        }
        int subSeq;
        uint16_t length;
        uint16_t packetLength = 0;
        for (subSeq = 0; subSeq < batchLength; subSeq ++) {
            auto startAddr = buffer + subSeq * totalLength;
            length = fread(startAddr + HEADER_LENGTH, 1, packetSize, fp);
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
        for (int i = 0; i < rsLength; i ++) {
            p[i] = buffer + (subSeq + i) * totalLength + HEADER_LENGTH;
        }
        for (int i = 0; i < subSeq; i ++) {
            p[i + rsLength] = buffer + i * totalLength + HEADER_LENGTH;
        }
        k->GenerateRSPacket(p, subSeq, rsLength, packetSize);
        uint8_t testBuffer[10000];
        for (int i = 0; i < subSeq + rsLength; i ++) {
            ++totalPackets;
            auto startAddr = buffer + i * totalLength;
            memcpy(testBuffer + i*packetSize, startAddr + HEADER_LENGTH, packetSize);
            header head = header(startAddr);
            head.SetRealSeq(subSeq);
            head.SetRSSeq(rsLength);
            head.SetSendSeq(seqNumber);
            head.SetPacketLength(packetLength);
            head.SetSubSeq(i);
            head.SetCRC(crc32c::Crc32c(startAddr + 4, totalLength - 4));
            sendto(UDPSock, startAddr, totalLength, 0,
                   reinterpret_cast<const sockaddr *>(&sSendAddr), sizeof(sockaddr_in));
        }
        fwrite(testBuffer, 1, packetLength, test);
        ++seqNumber;
    }
    fflush(test);
    finished.store(true);
    auto end =time(nullptr);
    printf("time=%f\n total:%d\n", difftime(end,start), totalPackets);
    if (ferror(fp)) {
        perror("read file");
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

void RDT(int fd, const sockaddr *sendAddr) {
    struct sockaddr_in  clientADDR{};
    socklen_t clientADDRLen = sizeof(clientADDR);
    uint8_t buffer[2000];
    uint8_t sendBuffer[2000];
    auto sendHead = header(sendBuffer);
    auto head = header(buffer);
    uint16_t length;
    fd_set FDSet;
    FD_ZERO(&FDSet);
    FD_SET(fd, &FDSet);
    timeval tv{0, 1000};
    started.store(true);
    while (true) {
        auto FDSetCopy = FDSet;
        auto TVCopy = tv;
        auto selectFD = select(fd + 1, &FDSetCopy, nullptr, nullptr, &TVCopy);
        if (selectFD < 0) {
            perror("socket");
            exit(0);
        } else if (selectFD == 0) {
            if (finished.load()) {
                sendHead.Clear();
                sendHead.SetSendSeq(seqNumber);
                sendHead.SetFIN(true);
                sendto(fd, sendBuffer, HEADER_LENGTH, 0, sendAddr, sizeof(sockaddr_in));
                std::cout << "send fin" << std::endl;
            }
            //todo exceed time val
        } else if (FD_ISSET(fd, &FDSetCopy)){
            length = recvfrom(fd, buffer, 10000, 0, reinterpret_cast<sockaddr *>(&clientADDR), &clientADDRLen);
            std::cout << head.AckSeq() << " " << length << std::endl;
            if (head.IsFin()) {
                std::cout << "client end";
                break;
            }
        }
    }
}