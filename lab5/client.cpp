#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace {

constexpr const char* kLicenseHost = "127.0.0.1";
constexpr const char* kStatusHost = "127.0.0.1";
constexpr int kLicensePort = 9000;
constexpr int kStatusPort = 9001;

std::string trim(std::string s) {
    const char* ws = " \t\r\n";
    auto begin = s.find_first_not_of(ws);
    if (begin == std::string::npos) return "";
    auto end = s.find_last_not_of(ws);
    return s.substr(begin, end - begin + 1);
}

bool send_line(SOCKET s, const std::string& line) {
    std::string data = line + "\n";
    const char* ptr = data.c_str();
    int remaining = static_cast<int>(data.size());
    while (remaining > 0) {
        int sent = send(s, ptr, remaining, 0);
        if (sent == SOCKET_ERROR) return false;
        ptr += sent;
        remaining -= sent;
    }
    return true;
}

bool recv_line(SOCKET s, std::string& out) {
    out.clear();
    char ch = 0;
    while (true) {
        int ret = recv(s, &ch, 1, 0);
        if (ret == 0) return false;
        if (ret == SOCKET_ERROR) return false;
        if (ch == '\n') break;
        if (ch != '\r') out.push_back(ch);
        if (out.size() > 4096) return false;
    }
    return true;
}

std::string make_client_id() {
    char computer[256] = {};
    DWORD size = sizeof(computer);
    if (!GetComputerNameA(computer, &size)) {
        std::strcpy(computer, "pc");
    }
    std::ostringstream oss;
    oss << computer << "-" << GetCurrentProcessId() << "-" << std::time(nullptr);
    return oss.str();
}

int infer_limit_from_serial(const std::string& serial) {
    if (serial.size() == 10 && serial[0] >= '5') {
        return 50;
    }
    return 10;
}

bool connect_socket(SOCKET& s, const char* host, int port) {
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        closesocket(s);
        s = INVALID_SOCKET;
        return false;
    }
    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        s = INVALID_SOCKET;
        return false;
    }
    DWORD timeout_ms = 5000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    return true;
}

bool request_license(std::string& serial, int& limit) {
    std::string username, password, type;
    std::cout << "username: ";
    std::getline(std::cin, username);
    std::cout << "password: ";
    std::getline(std::cin, password);
    std::cout << "type (10/50): ";
    std::getline(std::cin, type);
    username = trim(username);
    password = trim(password);
    type = trim(type);

    SOCKET s = INVALID_SOCKET;
    if (!connect_socket(s, kLicenseHost, kLicensePort)) {
        std::cout << "connect license server failed\n";
        return false;
    }

    std::ostringstream oss;
    oss << "ISSUE|" << username << "|" << password << "|" << type;
    if (!send_line(s, oss.str())) {
        closesocket(s);
        return false;
    }

    std::string resp;
    bool ok = recv_line(s, resp);
    closesocket(s);
    if (!ok) return false;

    std::istringstream iss(resp);
    std::string code, limit_str;
    std::getline(iss, code, '|');
    std::getline(iss, serial, '|');
    std::getline(iss, limit_str, '|');
    if (code != "OK") {
        std::cout << "license server rejected: " << resp << "\n";
        return false;
    }

    limit = std::stoi(limit_str);
    std::cout << "serial: " << serial << "\n";
    return true;
}

class StatusSession {
public:
    bool start(const std::string& serial, const std::string& client_id, int limit) {
        serial_ = serial;
        client_id_ = client_id;
        limit_ = limit;
        return reconnect();
    }

    void run() {
        std::cout << "press ENTER to exit\n";
        std::thread heartbeat([this] { heartbeat_loop(); });
        std::string dummy;
        std::getline(std::cin, dummy);
        stopping_ = true;
        send_logout();
        heartbeat.join();
    }

private:
    bool reconnect() {
        if (sock_ != INVALID_SOCKET) {
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
        }
        if (!connect_socket(sock_, kStatusHost, kStatusPort)) return false;

        std::ostringstream oss;
        oss << "AUTH|" << serial_ << "|" << client_id_ << "|" << limit_;
        if (!send_line(sock_, oss.str())) return false;

        std::string resp;
        if (!recv_line(sock_, resp)) return false;
        if (resp.rfind("ALLOW|", 0) != 0) {
            std::cout << "authorization failed: " << resp << "\n";
            return false;
        }
        std::cout << "authorization success: " << resp << "\n";
        return true;
    }

    void heartbeat_loop() {
        while (!stopping_) {
            for (int i = 0; i < 30 && !stopping_; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (stopping_) break;

            if (sock_ == INVALID_SOCKET && !reconnect()) {
                std::cout << "waiting for status server...\n";
                continue;
            }

            std::ostringstream oss;
            oss << "PING|" << serial_ << "|" << client_id_;
            if (!send_line(sock_, oss.str())) {
                handle_disconnect();
                continue;
            }

            std::string resp;
            if (!recv_line(sock_, resp)) {
                handle_disconnect();
                continue;
            }
            std::cout << "heartbeat: " << resp << "\n";
        }
    }

    void handle_disconnect() {
        if (sock_ != INVALID_SOCKET) {
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
        }
        std::cout << "status server disconnected, retrying...\n";
        while (!stopping_ && !reconnect()) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }

    void send_logout() {
        if (sock_ == INVALID_SOCKET) return;
        send_line(sock_, "LOGOUT|" + serial_ + "|" + client_id_);
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }

    SOCKET sock_ = INVALID_SOCKET;
    std::string serial_;
    std::string client_id_;
    std::atomic<bool> stopping_{false};
    int limit_ = 10;
};

}  // namespace

int main() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    std::cout << "1. request new license\n";
    std::cout << "2. use existing license\n";
    std::cout << "choice: ";
    std::string choice;
    std::getline(std::cin, choice);

    std::string serial;
    int limit = 10;
    if (trim(choice) == "1") {
        if (!request_license(serial, limit)) {
            WSACleanup();
            return 1;
        }
    } else {
        std::cout << "serial: ";
        std::getline(std::cin, serial);
        serial = trim(serial);
        limit = infer_limit_from_serial(serial);
    }

    std::string client_id = make_client_id();
    std::cout << "client id: " << client_id << "\n";
    std::cout << "license limit hint: " << limit << "\n";

    StatusSession session;
    if (!session.start(serial, client_id, limit)) {
        std::cout << "failed to connect/authenticate status server\n";
        WSACleanup();
        return 1;
    }

    session.run();

    WSACleanup();
    return 0;
}
