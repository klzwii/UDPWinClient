#include <ctime>
#include <iostream>
#include <random>
#include <thread>
#include <cstring>
#include <unistd.h>
#include<cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include "packetConst.h"
#include "RSHelper.h"
#include "crc32c/crc32c.h"
#define batchLength 5
#define packetSize 1000
#define rsLength 2
int main() {
    auto  k = new RSHelper();
    FILE *fp;
    if (!(fp = fopen("D:\\12345.pdf", "r"))) {
        perror("open file");
    }
    uint8_t buffer[10000];
    int UDPSock;
    if ((UDPSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("open socket");
    }
    uint16_t seqNumber = 0;
    struct sockaddr_in sRecvAddr{},sSendAddr{};
    sSendAddr.sin_family = AF_INET;
    sSendAddr.sin_addr.s_addr = inet_addr("159.75.92.163");
    sSendAddr.sin_port = htons(1234);
    auto start = time(nullptr);
    auto totalLength = packetSize + HEADER_LENGTH;
    uint8_t* p[10];
    while (true) {
        if (ferror(fp) || feof(fp)) {
            break;
        }
        int subSeq, length;
        for (subSeq = 0; subSeq < batchLength; subSeq ++) {
            if (feof(fp)) {
                break;
            }
            auto startAddr = buffer + subSeq * totalLength;
            length = fread(startAddr, 1, packetSize, fp);
            if (ferror(fp)) {
                break;
            }

        }
        memset(buffer + subSeq * totalLength + HEADER_LENGTH + length, 0, packetSize - length);
        for (int i = 0; i < rsLength; i ++) {
            p[i] = buffer + (subSeq + i) * totalLength + HEADER_LENGTH;
        }
        for (int i = 0; i < subSeq; i ++) {
            p[i + rsLength] = buffer + i * totalLength + HEADER_LENGTH;
        }
        k->GenerateRSPacket(p, subSeq, rsLength, packetSize);
        for (int i = 0; i < subSeq + rsLength; i ++) {
            auto startAddr = buffer + subSeq * totalLength;
            header head = header(startAddr, false);
            head.SetAllSeq(subSeq + rsLength);
            head.SetPacketLength(packetSize);
            head.SetCRC(crc32c::Crc32c(startAddr + 4, totalLength - 4));
            head.SetSendSeq(seqNumber);
            head.SetSubSeq(subSeq);
            //std::cout << "send to " << seqNumber << " " << i << " " <<  subSeq << std::endl;
            sendto(UDPSock, startAddr, totalLength, 0,
                   reinterpret_cast<const sockaddr *>(&sSendAddr), sizeof(sockaddr_in));
        }
        ++seqNumber;
    }
    auto end =time(nullptr);
    printf("time=%f\n",difftime(end,start));
    if (ferror(fp)) {
        perror("read file");
    }
}