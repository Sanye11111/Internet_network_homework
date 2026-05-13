#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

static constexpr size_t kDefaultChunkSize = 1200;
static constexpr int kMetaTimeoutSeconds = 8;
static constexpr int kDataTimeoutSeconds = 3;

struct PacketHeader {
    uint32_t seq;
    uint32_t len;
};

static void closeSocket(SOCKET s) {
    closesocket(s);
}

static string basenameOf(const string& path) {
    return filesystem::path(path).filename().string();
}

static bool sendPacket(SOCKET sock, const sockaddr_in& addr, const char* data, int len) {
    int sent = sendto(sock, data, len, 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    return sent == len;
}

static bool waitRecv(SOCKET sock, char* buf, int cap, int timeoutSeconds, int& n, sockaddr_in& from) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    timeval tv{};
    tv.tv_sec = timeoutSeconds;
    int ret = select(0, &fds, nullptr, nullptr, &tv);
    if (ret <= 0) {
        return false;
    }
    int fromLen = sizeof(from);
    n = recvfrom(sock, buf, cap, 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
    return n > 0;
}

static bool sendFile(const string& groupIp, int port, const string& inputPath, size_t chunkSize) {
    ifstream in(inputPath, ios::binary);
    if (!in) {
        cerr << "cannot open input file\n";
        return false;
    }

    in.seekg(0, ios::end);
    uint64_t fileSize = static_cast<uint64_t>(in.tellg());
    in.seekg(0, ios::beg);
    uint32_t totalChunks = static_cast<uint32_t>((fileSize + chunkSize - 1) / chunkSize);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        cerr << "socket failed\n";
        return false;
    }

    int ttl = 1;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<char*>(&ttl), sizeof(ttl));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(groupIp.c_str());
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        cerr << "bad multicast ip\n";
        closeSocket(sock);
        return false;
    }

    string meta = "META " + to_string(fileSize) + " " + to_string(chunkSize) + " " +
                  to_string(totalChunks) + " " + basenameOf(inputPath) + "\n";
    if (!sendPacket(sock, addr, meta.data(), static_cast<int>(meta.size()))) {
        cerr << "send meta failed\n";
        closeSocket(sock);
        return false;
    }

    vector<char> chunk(chunkSize);
    auto start = chrono::steady_clock::now();
    for (uint32_t seq = 0; seq < totalChunks; ++seq) {
        in.read(chunk.data(), static_cast<streamsize>(chunkSize));
        size_t got = static_cast<size_t>(in.gcount());
        PacketHeader hdr{htonl(seq), htonl(static_cast<uint32_t>(got))};
        vector<char> packet(sizeof(hdr) + got);
        memcpy(packet.data(), &hdr, sizeof(hdr));
        memcpy(packet.data() + sizeof(hdr), chunk.data(), got);
        if (!sendPacket(sock, addr, packet.data(), static_cast<int>(packet.size()))) {
            cerr << "send chunk failed\n";
            closeSocket(sock);
            return false;
        }
    }
    auto end = chrono::steady_clock::now();
    double sec = chrono::duration<double>(end - start).count();
    double rate = sec > 0 ? (fileSize / 1024.0 / 1024.0) / sec : 0.0;
    cout << "sent " << fileSize << " bytes in " << sec << " s, " << rate << " MB/s\n";

    closeSocket(sock);
    return true;
}

static bool recvFile(const string& groupIp, int port, const string& outputPath) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        cerr << "socket failed\n";
        return false;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuse), sizeof(reuse));

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(port);
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
        cerr << "bind failed\n";
        closeSocket(sock);
        return false;
    }

    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(groupIp.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<char*>(&mreq), sizeof(mreq)) != 0) {
        cerr << "join multicast failed\n";
        closeSocket(sock);
        return false;
    }

    vector<char> buf(2048);
    sockaddr_in from{};
    int n = 0;
    if (!waitRecv(sock, buf.data(), static_cast<int>(buf.size()), kMetaTimeoutSeconds, n, from)) {
        cerr << "meta timeout\n";
        closeSocket(sock);
        return false;
    }

    string meta(buf.data(), buf.data() + n);
    istringstream iss(meta);
    string tag, name;
    uint64_t fileSize = 0;
    size_t chunkSize = 0;
    uint32_t totalChunks = 0;
    iss >> tag >> fileSize >> chunkSize >> totalChunks >> name;
    if (tag != "META" || chunkSize == 0 || totalChunks == 0) {
        cerr << "bad meta\n";
        closeSocket(sock);
        return false;
    }

    ofstream out(outputPath, ios::binary | ios::trunc);
    if (!out) {
        cerr << "cannot open output file\n";
        closeSocket(sock);
        return false;
    }
    if (fileSize > 0) {
        out.seekp(static_cast<streamoff>(fileSize - 1));
        out.put('\0');
        out.flush();
    }

    vector<bool> got(totalChunks, false);
    size_t received = 0;
    auto start = chrono::steady_clock::now();

    while (received < totalChunks) {
        if (!waitRecv(sock, buf.data(), static_cast<int>(buf.size()), kDataTimeoutSeconds, n, from)) {
            break;
        }
        if (n < static_cast<int>(sizeof(PacketHeader))) {
            continue;
        }

        PacketHeader hdr{};
        memcpy(&hdr, buf.data(), sizeof(hdr));
        uint32_t seq = ntohl(hdr.seq);
        uint32_t len = ntohl(hdr.len);
        if (seq >= totalChunks || len > static_cast<uint32_t>(n - sizeof(hdr))) {
            continue;
        }
        if (got[seq]) {
            continue;
        }

        out.seekp(static_cast<streamoff>(seq) * static_cast<streamoff>(chunkSize));
        out.write(buf.data() + sizeof(hdr), len);
        got[seq] = true;
        ++received;
    }

    auto end = chrono::steady_clock::now();
    double sec = chrono::duration<double>(end - start).count();
    double rate = sec > 0 ? (fileSize / 1024.0 / 1024.0) / sec : 0.0;
    cout << "received " << received << "/" << totalChunks << " chunks in " << sec
         << " s, " << rate << " MB/s\n";

    closeSocket(sock);
    return received == totalChunks;
}

int main(int argc, char* argv[]) {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    if (argc < 5) {
        cerr << "usage:\n";
        cerr << "  send <group_ip> <port> <input_file> [chunk_size]\n";
        cerr << "  recv <group_ip> <port> <output_file>\n";
        WSACleanup();
        return 1;
    }

    string mode = argv[1];
    string groupIp = argv[2];
    int port = stoi(argv[3]);

    int rc = 1;
    if (mode == "send") {
        size_t chunkSize = argc >= 6 ? static_cast<size_t>(stoul(argv[5])) : kDefaultChunkSize;
        rc = sendFile(groupIp, port, argv[4], chunkSize) ? 0 : 1;
    } else if (mode == "recv") {
        rc = recvFile(groupIp, port, argv[4]) ? 0 : 1;
    } else {
        cerr << "unknown mode\n";
        rc = 1;
    }

    WSACleanup();
    return rc;
}
