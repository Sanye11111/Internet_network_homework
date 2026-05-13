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

static constexpr size_t kDefaultChunkSize = 1400;

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

static bool sendAllTcp(SOCKET sock, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = send(sock, data + sent, static_cast<int>(len - sent), 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

static bool recvExactTcp(SOCKET sock, char* data, size_t len) {
    size_t got = 0;
    while (got < len) {
        int n = recv(sock, data + got, static_cast<int>(len - got), 0);
        if (n <= 0) {
            return false;
        }
        got += static_cast<size_t>(n);
    }
    return true;
}

static bool recvLineTcp(SOCKET sock, string& line) {
    line.clear();
    char ch = 0;
    while (true) {
        int n = recv(sock, &ch, 1, 0);
        if (n <= 0) {
            return false;
        }
        if (ch == '\n') {
            break;
        }
        line.push_back(ch);
    }
    return true;
}

static bool sendUdpPacket(SOCKET sock, const sockaddr_in& addr, const char* data, int len) {
    int n = sendto(sock, data, len, 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    return n == len;
}

static bool recvUdpPacket(SOCKET sock, vector<char>& buf, int timeoutSeconds, int& n, sockaddr_in& from) {
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
    n = recvfrom(sock, buf.data(), static_cast<int>(buf.size()), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
    return n > 0;
}

static bool tcpSendFile(const string& host, int port, const string& filePath, size_t chunkSize) {
    ifstream in(filePath, ios::binary);
    if (!in) {
        cerr << "cannot open input file\n";
        return false;
    }

    in.seekg(0, ios::end);
    uint64_t fileSize = static_cast<uint64_t>(in.tellg());
    in.seekg(0, ios::beg);
    uint32_t totalChunks = static_cast<uint32_t>((fileSize + chunkSize - 1) / chunkSize);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        cerr << "socket failed\n";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        cerr << "bad ip\n";
        closeSocket(sock);
        return false;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        cerr << "connect failed\n";
        closeSocket(sock);
        return false;
    }

    string meta = "META " + to_string(fileSize) + " " + to_string(chunkSize) + " " +
                  to_string(totalChunks) + "\n";
    if (!sendAllTcp(sock, meta.data(), meta.size())) {
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
        if (!sendAllTcp(sock, reinterpret_cast<char*>(&hdr), sizeof(hdr)) ||
            !sendAllTcp(sock, chunk.data(), got)) {
            cerr << "send chunk failed\n";
            closeSocket(sock);
            return false;
        }
    }
    auto end = chrono::steady_clock::now();
    double sec = chrono::duration<double>(end - start).count();
    double rate = sec > 0 ? (fileSize / 1024.0 / 1024.0) / sec : 0.0;
    cout << "tcp sent " << fileSize << " bytes in " << sec << " s, " << rate << " MB/s\n";

    closeSocket(sock);
    return true;
}

static bool tcpRecvFile(int port, const string& outputPath) {
    SOCKET lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (lsock == INVALID_SOCKET) {
        cerr << "socket failed\n";
        return false;
    }

    int reuse = 1;
    setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(lsock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        cerr << "bind failed\n";
        closeSocket(lsock);
        return false;
    }
    if (listen(lsock, 1) != 0) {
        cerr << "listen failed\n";
        closeSocket(lsock);
        return false;
    }

    sockaddr_in cli{};
    int cliLen = sizeof(cli);
    SOCKET sock = accept(lsock, reinterpret_cast<sockaddr*>(&cli), &cliLen);
    closeSocket(lsock);
    if (sock == INVALID_SOCKET) {
        cerr << "accept failed\n";
        return false;
    }

    string meta;
    if (!recvLineTcp(sock, meta)) {
        cerr << "recv meta failed\n";
        closeSocket(sock);
        return false;
    }

    uint64_t fileSize = 0;
    size_t chunkSize = 0;
    uint32_t totalChunks = 0;
    {
        string tag;
        istringstream iss(meta);
        iss >> tag >> fileSize >> chunkSize >> totalChunks;
        if (tag != "META" || chunkSize == 0 || totalChunks == 0) {
            cerr << "bad meta\n";
            closeSocket(sock);
            return false;
        }
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

    vector<char> chunk(chunkSize);
    auto start = chrono::steady_clock::now();
    for (uint32_t seq = 0; seq < totalChunks; ++seq) {
        PacketHeader hdr{};
        if (!recvExactTcp(sock, reinterpret_cast<char*>(&hdr), sizeof(hdr))) {
            cerr << "recv header failed\n";
            closeSocket(sock);
            return false;
        }
        uint32_t len = ntohl(hdr.len);
        if (len > chunkSize) {
            cerr << "bad len\n";
            closeSocket(sock);
            return false;
        }
        if (!recvExactTcp(sock, chunk.data(), len)) {
            cerr << "recv chunk failed\n";
            closeSocket(sock);
            return false;
        }
        out.seekp(static_cast<streamoff>(seq) * static_cast<streamoff>(chunkSize));
        out.write(chunk.data(), len);
    }
    auto end = chrono::steady_clock::now();
    double sec = chrono::duration<double>(end - start).count();
    double rate = sec > 0 ? (fileSize / 1024.0 / 1024.0) / sec : 0.0;
    cout << "tcp received " << fileSize << " bytes in " << sec << " s, " << rate << " MB/s\n";

    closeSocket(sock);
    return true;
}

static bool udpSendFile(const string& host, int port, const string& filePath, size_t chunkSize) {
    ifstream in(filePath, ios::binary);
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

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        cerr << "bad ip\n";
        closeSocket(sock);
        return false;
    }

    string meta = "META " + to_string(fileSize) + " " + to_string(chunkSize) + " " +
                  to_string(totalChunks) + "\n";
    if (!sendUdpPacket(sock, addr, meta.data(), static_cast<int>(meta.size()))) {
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
        if (!sendUdpPacket(sock, addr, packet.data(), static_cast<int>(packet.size()))) {
            cerr << "send chunk failed\n";
            closeSocket(sock);
            return false;
        }
    }
    auto end = chrono::steady_clock::now();
    double sec = chrono::duration<double>(end - start).count();
    double rate = sec > 0 ? (fileSize / 1024.0 / 1024.0) / sec : 0.0;
    cout << "udp sent " << fileSize << " bytes in " << sec << " s, " << rate << " MB/s\n";

    closeSocket(sock);
    return true;
}

static bool udpRecvFile(int port, const string& outputPath) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        cerr << "socket failed\n";
        return false;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        cerr << "bind failed\n";
        closeSocket(sock);
        return false;
    }

    vector<char> buf(2048);
    sockaddr_in from{};
    int n = 0;
    if (!recvUdpPacket(sock, buf, 8, n, from)) {
        cerr << "meta timeout\n";
        closeSocket(sock);
        return false;
    }

    string meta(buf.data(), buf.data() + n);
    istringstream iss(meta);
    string tag;
    uint64_t fileSize = 0;
    size_t chunkSize = 0;
    uint32_t totalChunks = 0;
    iss >> tag >> fileSize >> chunkSize >> totalChunks;
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
        if (!recvUdpPacket(sock, buf, 3, n, from)) {
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
    cout << "udp received " << received << "/" << totalChunks << " chunks in "
         << sec << " s, " << rate << " MB/s\n";

    closeSocket(sock);
    return received == totalChunks;
}

int main(int argc, char* argv[]) {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    if (argc < 4) {
        cerr << "usage:\n";
        cerr << "  tcp-send <ip> <port> <file> [chunk_size]\n";
        cerr << "  tcp-recv <port> <output_file>\n";
        cerr << "  udp-send <ip> <port> <file> [chunk_size]\n";
        cerr << "  udp-recv <port> <output_file>\n";
        WSACleanup();
        return 1;
    }

    string mode = argv[1];
    int rc = 1;
    if (mode == "tcp-send" || mode == "udp-send") {
        if (argc < 5) {
            WSACleanup();
            return 1;
        }
        string ip = argv[2];
        int port = stoi(argv[3]);
        string file = argv[4];
        size_t chunkSize = argc >= 6 ? static_cast<size_t>(stoul(argv[5])) : kDefaultChunkSize;
        rc = (mode == "tcp-send") ? (tcpSendFile(ip, port, file, chunkSize) ? 0 : 1)
                                  : (udpSendFile(ip, port, file, chunkSize) ? 0 : 1);
    } else if (mode == "tcp-recv" || mode == "udp-recv") {
        if (argc < 4) {
            WSACleanup();
            return 1;
        }
        int port = stoi(argv[2]);
        string output = argv[3];
        rc = (mode == "tcp-recv") ? (tcpRecvFile(port, output) ? 0 : 1)
                                  : (udpRecvFile(port, output) ? 0 : 1);
    } else {
        cerr << "unknown mode\n";
        rc = 1;
    }

    WSACleanup();
    return rc;
}
