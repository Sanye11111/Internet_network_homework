import os
import socket
import struct
import sys
import time

ICMP_ECHO_REQUEST = 8
ICMP_ECHO_REPLY = 0


def checksum(data):
    total = 0
    count = 0
    while count + 1 < len(data):
        total += data[count] + (data[count + 1] << 8)
        count += 2
    if count < len(data):
        total += data[count]
    total = (total >> 16) + (total & 0xffff)
    total += total >> 16
    return (~total) & 0xffff


def build_packet(seq, payload):
    header = struct.pack("!BBHHH", ICMP_ECHO_REQUEST, 0, 0, os.getpid() & 0xffff, seq)
    data = payload.encode("utf-8")
    value = checksum(header + data)
    header = struct.pack("!BBHHH", ICMP_ECHO_REQUEST, 0, value, os.getpid() & 0xffff, seq)
    return header + data


def run_client(host):
    sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_ICMP)
    packet = build_packet(1, "TIME")
    start = time.time()
    sock.sendto(packet, (host, 0))
    data, addr = sock.recvfrom(1500)
    used = (time.time() - start) * 1000
    ip_header_len = (data[0] & 0x0f) * 4
    icmp_data = data[ip_header_len + 8 :].decode("utf-8", errors="ignore")
    print("reply from", addr[0], "time =", icmp_data, "rtt = %.2fms" % used)


def run_server():
    sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_ICMP)
    print("icmp time server started")
    while True:
        data, addr = sock.recvfrom(1500)
        ip_header_len = (data[0] & 0x0f) * 4
        icmp = data[ip_header_len:]
        icmp_type, code, _, ident, seq = struct.unpack("!BBHHH", icmp[:8])
        if icmp_type != ICMP_ECHO_REQUEST:
            continue
        now = time.strftime("%Y-%m-%d %H:%M:%S")
        payload = now.encode("utf-8")
        header = struct.pack("!BBHHH", ICMP_ECHO_REPLY, code, 0, ident, seq)
        value = checksum(header + payload)
        header = struct.pack("!BBHHH", ICMP_ECHO_REPLY, code, value, ident, seq)
        sock.sendto(header + payload, addr)


if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "server":
        run_server()
    elif len(sys.argv) >= 3 and sys.argv[1] == "client":
        run_client(sys.argv[2])
    else:
        print("usage:")
        print("  python q22_icmp_time.py server")
        print("  python q22_icmp_time.py client 127.0.0.1")
