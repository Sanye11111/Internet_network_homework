#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>

#define MAC_ADDRESS_LENGTH 6

typedef uint8_t MAC_address[MAC_ADDRESS_LENGTH];

struct EthernetFrame {
    MAC_address destination;
    MAC_address source;
    uint16_t type;
    uint8_t payload[1500];
    uint32_t fcs;
};

MAC_address this_mac_address = {0x00, 0x50, 0x56, 0xC0, 0x00, 0x01};

int mac_address_match(const struct EthernetFrame *frame) {
    static const MAC_address broadcast = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

    if (std::memcmp(frame->destination, this_mac_address, MAC_ADDRESS_LENGTH) == 0) {
        return 1;
    }

    if (std::memcmp(frame->destination, broadcast, MAC_ADDRESS_LENGTH) == 0) {
        return 1;
    }

    if ((frame->destination[0] & 0x01) != 0) {
        return 1;
    }

    return 0;
}

void print_mac(const MAC_address &mac) {
    for (int i = 0; i < MAC_ADDRESS_LENGTH; ++i) {
        if (i > 0) {
            std::cout << ":";
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(mac[i]);
    }
    std::cout << std::dec;
}

void test_frame(const MAC_address &dst, const char *label) {
    EthernetFrame frame = {};
    std::memcpy(frame.destination, dst, MAC_ADDRESS_LENGTH);

    std::cout << label << " 目的地址 ";
    print_mac(frame.destination);
    std::cout << " -> " << (mac_address_match(&frame) ? "接收" : "丢弃") << "\n";
}

int main() {
    const MAC_address local_dst = {0x00, 0x50, 0x56, 0xC0, 0x00, 0x01};
    const MAC_address multicast_dst = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};
    const MAC_address broadcast_dst = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const MAC_address other_dst = {0x00, 0x16, 0x3E, 0x12, 0x34, 0x56};

    test_frame(local_dst, "本机单播");
    test_frame(multicast_dst, "组播");
    test_frame(broadcast_dst, "广播");
    test_frame(other_dst, "其他主机");

    return 0;
}
