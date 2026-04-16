#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

typedef struct {
    unsigned char op;
    unsigned char htype;
    unsigned char hlen;
    unsigned char hops;
    unsigned int xid;
    unsigned short secs;
    unsigned short flags;
    unsigned int ciaddr;
    unsigned int yiaddr;
    unsigned int siaddr;
    unsigned int giaddr;
    unsigned char chaddr[16];
    unsigned char sname[64];
    unsigned char file[128];
    unsigned char options[312];
} DhcpPacket;

static int get_message_type(unsigned char *options, int len) {
    int i = 4;
    while (i < len) {
        if (options[i] == 255) {
            break;
        }
        if (options[i] == 53) {
            return options[i + 2];
        }
        i += options[i + 1] + 2;
    }
    return -1;
}

static void fill_packet(DhcpPacket *pkt, const DhcpPacket *req, int msgType) {
    unsigned int serverIp = inet_addr("192.168.1.1");
    unsigned int clientIp = inet_addr("192.168.1.2");
    unsigned int mask = inet_addr("255.255.255.0");

    memset(pkt, 0, sizeof(DhcpPacket));
    pkt->op = 2;
    pkt->htype = 1;
    pkt->hlen = 6;
    pkt->xid = req->xid;
    pkt->flags = req->flags;
    pkt->yiaddr = clientIp;
    pkt->siaddr = serverIp;
    memcpy(pkt->chaddr, req->chaddr, 16);

    pkt->options[0] = 99;
    pkt->options[1] = 130;
    pkt->options[2] = 83;
    pkt->options[3] = 99;

    pkt->options[4] = 53;
    pkt->options[5] = 1;
    pkt->options[6] = (unsigned char)msgType;

    pkt->options[7] = 54;
    pkt->options[8] = 4;
    memcpy(&pkt->options[9], &serverIp, 4);

    pkt->options[13] = 1;
    pkt->options[14] = 4;
    memcpy(&pkt->options[15], &mask, 4);

    pkt->options[19] = 3;
    pkt->options[20] = 4;
    memcpy(&pkt->options[21], &serverIp, 4);

    pkt->options[25] = 6;
    pkt->options[26] = 4;
    memcpy(&pkt->options[27], &serverIp, 4);

    pkt->options[31] = 51;
    pkt->options[32] = 4;
    pkt->options[33] = 0;
    pkt->options[34] = 0;
    pkt->options[35] = 0x0e;
    pkt->options[36] = 0x10;

    pkt->options[37] = 255;
}

int main(void) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    int opt = 1;
    struct sockaddr_in serverAddr;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    if (sockfd < 0) {
        printf("socket create failed\n");
        return 1;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt));

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DHCP_SERVER_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        printf("bind failed, maybe need admin privilege\n");
        return 1;
    }

    printf("simple DHCP server is running...\n");

    while (1) {
        DhcpPacket req;
        struct sockaddr_in dest;
#ifdef _WIN32
        int len = sizeof(dest);
#else
        socklen_t len = sizeof(dest);
#endif

        int n = recvfrom(sockfd, (char *)&req, sizeof(req), 0,
                         (struct sockaddr *)&dest, &len);
        if (n <= 0) {
            continue;
        }

        {
            int type = get_message_type(req.options, sizeof(req.options));
            DhcpPacket reply;

            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(DHCP_CLIENT_PORT);
            dest.sin_addr.s_addr = INADDR_BROADCAST;

            if (type == 1) {
                fill_packet(&reply, &req, 2);
                sendto(sockfd, (const char *)&reply, sizeof(reply), 0,
                       (struct sockaddr *)&dest, sizeof(dest));
                printf("send DHCPOFFER: 192.168.1.2\n");
            } else if (type == 3) {
                fill_packet(&reply, &req, 5);
                sendto(sockfd, (const char *)&reply, sizeof(reply), 0,
                       (struct sockaddr *)&dest, sizeof(dest));
                printf("send DHCPACK: 192.168.1.2\n");
            }
        }
    }
}
