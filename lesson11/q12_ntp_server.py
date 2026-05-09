import socket
import struct
import sys
import time
from datetime import datetime, timezone

NTP_DELTA = 2208988800


def to_ntp_time(dt):
    seconds = dt.replace(tzinfo=timezone.utc).timestamp()
    ntp_seconds = int(seconds + NTP_DELTA)
    fraction = int((seconds - int(seconds)) * (2**32))
    return ntp_seconds, fraction


def build_reply(request, dt):
    receive_sec, receive_frac = to_ntp_time(dt)
    transmit_sec, transmit_frac = receive_sec, receive_frac

    reply = bytearray(48)
    reply[0] = 0x1C
    reply[1] = 1
    reply[2] = 0
    reply[3] = 0xFA
    struct.pack_into("!I", reply, 4, 1 << 16)
    struct.pack_into("!I", reply, 8, 1 << 16)
    reply[12:16] = b"LOCL"

    if len(request) >= 48:
        reply[24:32] = request[40:48]

    struct.pack_into("!II", reply, 32, receive_sec, receive_frac)
    struct.pack_into("!II", reply, 40, transmit_sec, transmit_frac)
    return bytes(reply)


def main():
    if len(sys.argv) < 2:
        print('usage: python q12_ntp_server.py "2019-05-01 10:41:00"')
        return

    fixed_time = datetime.strptime(sys.argv[1], "%Y-%m-%d %H:%M:%S")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", 123))
    print("ntp server started, fixed time =", sys.argv[1])

    while True:
        data, addr = sock.recvfrom(1024)
        reply = build_reply(data, fixed_time)
        sock.sendto(reply, addr)


if __name__ == "__main__":
    main()
