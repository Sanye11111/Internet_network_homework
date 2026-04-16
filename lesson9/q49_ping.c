#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

typedef unsigned short u16;

static u16 checksum(void *data, int len) {
    unsigned int sum = 0;
    u16 *p = (u16 *)data;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(unsigned char *)p;
    }
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (u16)(~sum);
}

static int run_server(void) {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        printf("create raw socket failed\n");
        return 1;
    }

    printf("ICMP echo server is running...\n");

    while (1) {
        char buffer[1500];
        struct sockaddr_in clientAddr;
#ifdef _WIN32
        int addrLen = sizeof(clientAddr);
#else
        socklen_t addrLen = sizeof(clientAddr);
#endif
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                         (struct sockaddr *)&clientAddr, &addrLen);
        if (n <= 0) {
            continue;
        }

        {
            int ipHeaderLen = (buffer[0] & 0x0f) * 4;
            unsigned char *icmp = (unsigned char *)(buffer + ipHeaderLen);
            if (n < ipHeaderLen + 8 || icmp[0] != 8) {
                continue;
            }

            icmp[0] = 0;
            icmp[2] = 0;
            icmp[3] = 0;
            {
                u16 sum = checksum(icmp, n - ipHeaderLen);
                icmp[2] = (unsigned char)(sum >> 8);
                icmp[3] = (unsigned char)(sum & 0xff);
            }

            sendto(sockfd, (const char *)icmp, n - ipHeaderLen, 0,
                   (struct sockaddr *)&clientAddr, addrLen);
        }
    }
}

static int run_client(const char *ip) {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    unsigned char packet[64];
    unsigned char buffer[1500];
    struct sockaddr_in target, from;
#ifdef _WIN32
    int fromLen = sizeof(from);
#else
    socklen_t fromLen = sizeof(from);
#endif

    if (sockfd < 0) {
        printf("create raw socket failed\n");
        return 1;
    }

    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr(ip);

    memset(packet, 0, sizeof(packet));
    packet[0] = 8;
    packet[4] = 0x12;
    packet[5] = 0x34;
    packet[7] = 0x01;
    strcpy((char *)(packet + 8), "hello icmp");

    {
        u16 sum = checksum(packet, sizeof(packet));
        packet[2] = (unsigned char)(sum >> 8);
        packet[3] = (unsigned char)(sum & 0xff);
    }

    {
        clock_t start = clock();
        if (sendto(sockfd, (const char *)packet, sizeof(packet), 0,
                   (struct sockaddr *)&target, sizeof(target)) < 0) {
            printf("send failed\n");
            return 1;
        }

        if (recvfrom(sockfd, (char *)buffer, sizeof(buffer), 0,
                     (struct sockaddr *)&from, &fromLen) > 0) {
            clock_t end = clock();
            double rtt = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;
            printf("reply from %s, time=%.2f ms\n", inet_ntoa(from.sin_addr), rtt);
        } else {
            printf("no reply\n");
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    if (argc >= 2 && strcmp(argv[1], "server") == 0) {
        return run_server();
    }

    if (argc >= 3 && strcmp(argv[1], "client") == 0) {
        return run_client(argv[2]);
    }

    printf("usage:\n");
    printf("  %s server\n", argv[0]);
    printf("  %s client <ip>\n", argv[0]);
    return 0;
}
