#include <stdint.h>

#define MAC_ADDRESS_LENGTH 6

struct ethernet_frame_header {
    uint8_t destination[MAC_ADDRESS_LENGTH];
    uint8_t source[MAC_ADDRESS_LENGTH];
    uint16_t type_or_length;
};
