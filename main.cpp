#include <ctime>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/ip.h>
#include <ifaddrs.h>
#include "HeaderConst.h"
#include <arpa/inet.h>
#include <thread>
#include <sys/socket.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <libnetfilter_queue/libnetfilter_queue_ipv4.h>
#include <linux/netfilter.h>
struct sockaddr_in sin{}, sout{};

void printIP(uint32_t netIP) {
    auto hostIP = be32toh(netIP);
    auto *k = (uint8_t*)&netIP;
    for (int i = 0; i <= 3; i ++) {
        printf("%d.", (int)*(k+i));
    }
    printf("\n");
}

int fd;

bool checkIPOut(struct iphdr* iph) {
    auto k = be32toh(iph->daddr);
    auto *t = (uint8_t*)&k;
    auto fir = *(t+3), sec = *(t+2);
    if (fir == 127 || fir == 10) {
        return false;
    }
    if (fir == 192 && sec == 168) {
        return false;
    }
    if (fir == 169 && sec == 254) {
        return false;
    }
    if (fir == 172 && sec >= 26 && sec <= 31) {
        return false;
    }
    if (fir >= 224) {
        return false;
    }
    return true;
}

int cb(struct nfq_q_handle *gh, struct nfgenmsg *nfmsg, struct nfq_data *nfad, void *data) {
    struct iphdr *iph;
    struct nfqnl_msg_packet_hdr *ph;
    ph = nfq_get_msg_packet_hdr(nfad);
    unsigned char* payload;
    if (ph) {
        uint32_t id = ntohl(ph->packet_id);
        int r = nfq_get_payload(nfad, &payload);
        if (r >= sizeof(struct iphdr)) {
            iph = reinterpret_cast<struct iphdr*>(payload);
            if (!checkIPOut(iph)) {
                return nfq_set_verdict(gh, id, NF_ACCEPT, 0, nullptr);
            }
            std::string s;
            switch (iph->protocol) {
                case IPPROTO_ICMP:
                    s = "icmp";
                    break;
                case IPPROTO_UDP:
                    s = "udp";
                    break;
            }
            printf("get %s packets, length %d\n", s.c_str(), r);
            printIP(iph->saddr);
            printIP(iph->daddr);
            send(fd, payload, r, 0);
            nfq_set_verdict(gh, id, NF_DROP, 0, nullptr);
        }
    }
    return 0;
}

int getSubnetMask()
{
    struct sockaddr_in *sin = NULL;

    struct ifaddrs *ifa = NULL, *ifList;

    if (getifaddrs(&ifList) < 0)
    {
        return -1;
    }

    for (ifa = ifList; ifa != NULL; ifa = ifa->ifa_next)
    {
        if(ifa->ifa_addr->sa_family == AF_INET)
        {
            printf("n>>> interfaceName: %s\n", ifa->ifa_name);

            sin = (struct sockaddr_in *)ifa->ifa_addr;
            printf(">>> ipAddress: %s\n", inet_ntoa(sin->sin_addr));

            sin = (struct sockaddr_in *)ifa->ifa_dstaddr;
            printf(">>> broadcast: %s\n", inet_ntoa(sin->sin_addr));

            sin = (struct sockaddr_in *)ifa->ifa_netmask;
            printf(">>> subnetMask: %s\n", inet_ntoa(sin->sin_addr));
        }
    }

    freeifaddrs(ifList);
    return 0;
}

void reCalcChecksum(uint16_t *buffer, size_t len) {
    auto *iph = (iphdr*)buffer;
    iph->check = 0;
    uint32_t cksum = 0;
    while(len)
    {
        cksum += *(buffer++);
        len -= 2;
    }
    while (cksum >> 16) {
        uint16_t t = cksum >> 16;
        cksum &= 0x0000ffff;
        cksum += t;
    }
    iph->check = ~cksum;
}

void readFromFD() {
    int buffer[2000];
    int len;
    int rawFD = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    struct sockaddr_in t{};
    t.sin_family = AF_INET;
    t.sin_addr.s_addr = inet_addr("192.168.72.129");
    while ((len = recv(fd, buffer, 2000, 0)) > 0) {
        printf("get packets len %d\n", len);
        auto *iph = (iphdr*)buffer;
        iph->daddr = inet_addr("192.168.72.129");
        reCalcChecksum((uint16_t*)buffer, len);
        sendto(rawFD, buffer, len, 0, (sockaddr*)&t, sizeof(sockaddr_in));
    }
    if (len < 0) {
        perror("recv");
    }
}

int main() {
    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        perror("open socket");
        exit(0);
    }
    sin.sin_port = htons(1234);
    sin.sin_addr.s_addr = inet_addr("159.75.92.163");
    sin.sin_family = AF_INET;
    sout.sin_addr.s_addr = htonl(INADDR_ANY);
    sout.sin_family = AF_INET;
    sout.sin_port = htons(0);
    auto c = bind(fd, (sockaddr *)&sout, sizeof(struct sockaddr_in));
    if (c < 0) {
        perror("bind");
        exit(0);
    }
    c = connect(fd, (sockaddr *)&sin, sizeof(struct sockaddr_in));
    if (c < 0) {
        perror("connect");
        exit(0);
    }
    struct nfq_handle *h;
    struct nfq_q_handle *qh;
    int r;
    char buf[10240];
    //auto f = popen("iptables -A OUTPUT -j NFQUEUE 1234", "r");
    h = nfq_open();
    if (h == NULL) {
        perror("nfq_open error");
        exit(0);
    }

    if (nfq_unbind_pf(h, AF_INET) != 0) {
        perror("nfq_unbind_pf error");
        exit(0);
    }
    if (nfq_bind_pf(h, AF_INET) != 0) {
        perror("nfq_bind_pf error");
        exit(0);
    }
    qh = nfq_create_queue(h, 1234, &cb, NULL);
    if (qh == NULL) {
        perror("nfq_create_queue error");
        exit(0);
    }

    if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) != 0) {
        perror("nfq_set_mod error");
        exit(0);
    }
    int queueFD = nfq_fd(h);
    fd_set oriFD;
    FD_ZERO(&oriFD);
    FD_SET(queueFD, &oriFD);
    timeval tv{1, 0};
    std::thread(readFromFD).detach();
    while(true) {
        auto fdCopy = oriFD;
        auto tvCopy = tv;
        auto c = select(queueFD + 1, &fdCopy, nullptr, nullptr, &tvCopy);
        if (c == 0) {
            continue;
        }
        if (FD_ISSET(queueFD, &fdCopy)) {
            r = recv(queueFD, buf, sizeof(buf), MSG_DONTWAIT);
            if (r == 0) {
                printf("recv return 0. exit");
                break;
            } else if (r < 0) {
                perror("recv error");
                break;
            } else {
                nfq_handle_packet(h, buf, r);
            }
        }
    }
    return 0;
}
