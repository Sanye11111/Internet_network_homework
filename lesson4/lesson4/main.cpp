#include <iostream>
using namespace std;

int parity_check(const unsigned char *msg, const int msg_length) {
    if (msg == 0 || msg_length <= 0) {
        return 0;
    }

    int ones = 0;
    for (int i = 0; i < msg_length; ++i) {
        if (msg[i] != 0) {
            ++ones;
        }
    }

    return (ones % 2 == 0) ? 1 : 0;
}