#include <stdio.h>

int is_in_net(unsigned char *ip, unsigned char *netip, unsigned char *mask) {
    int i;
    for (i = 0; i < 4; ++i) {
        if ((ip[i] & mask[i]) != (netip[i] & mask[i])) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    unsigned char ip[4] = {192, 168, 1, 66};
    unsigned char netip[4] = {192, 168, 1, 0};
    unsigned char mask[4] = {255, 255, 255, 0};

    if (is_in_net(ip, netip, mask)) {
        printf("match\n");
    } else {
        printf("not match\n");
    }

    return 0;
}
