#include <stdio.h>

int classwise(unsigned char *ip) {
    if (ip[0] <= 127) {
        return 0;
    }
    if (ip[0] <= 191) {
        return 1;
    }
    if (ip[0] <= 223) {
        return 2;
    }
    if (ip[0] <= 239) {
        return 3;
    }
    return 4;
}

int main(void) {
    unsigned char ip[4] = {224, 1, 2, 3};
    printf("%d\n", classwise(ip));
    return 0;
}
