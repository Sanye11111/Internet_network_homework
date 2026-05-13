#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>

namespace {

constexpr int kPort = 9000;
constexpr int kBacklog = SOMAXCONN;

std::mutex g_log_mutex;
std::unordered_set<std::string> g_serials;
std::mt19937 g_rng{std::random_device{}()};

std::string now_string() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto tt = system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &tt);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << "[" << now_string() << "] " << msg << std::endl;
}

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

std::string generate_serial_for_type(const std::string& type) {
    std::uniform_int_distribution<int> low_digit(0, 4);
    std::uniform_int_distribution<int> high_digit(5, 9);
    std::uniform_int_distribution<int> digit(0, 9);
    for (;;) {
        std::string serial;
        serial.reserve(10);
        int first = (type == "50" || type == "large") ? high_digit(g_rng) : low_digit(g_rng);
        serial.push_back(static_cast<char>('0' + first));
        for (int i = 1; i < 10; ++i) {
            serial.push_back(static_cast<char>('0' + digit(g_rng)));
        }
        if (g_serials.insert(serial).second) return serial;
    }
}

int license_limit_for_type(const std::string& type) {
    if (type == "50" || type == "large") return 50;
    return 10;
}

DWORD WINAPI client_thread(LPVOID param) {
    SOCKET client = reinterpret_cast<SOCKET>(param);
    std::string request;
    std::string response;
    if (!recv_line(client, request)) {
        closesocket(client);
        return 0;
    }

    std::istringstream iss(request);
    std::string cmd, username, password, type;
    std::getline(iss, cmd, '|');
    std::getline(iss, username, '|');
    std::getline(iss, password, '|');
    std::getline(iss, type, '|');

    cmd = trim(cmd);
    username = trim(username);
    password = trim(password);
    type = trim(type);

    if (cmd != "ISSUE" || username.empty() || password.empty() || type.empty()) {
        response = "ERR|invalid_request";
    } else {
        const std::string serial = generate_serial_for_type(type);
        const int limit = license_limit_for_type(type);
        std::ostringstream oss;
        oss << "OK|" << serial << "|" << limit;
        response = oss.str();
        log("issued serial " + serial + " to " + username + " (type=" + type + ", limit=" + std::to_string(limit) + ")");
    }

    send_line(client, response);
    shutdown(client, SD_BOTH);
    closesocket(client);
    return 0;
}

}  // namespace

int main() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        std::cerr << "socket failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kPort);

    if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind failed\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    if (listen(server, kBacklog) == SOCKET_ERROR) {
        std::cerr << "listen failed\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    log("license server started on port " + std::to_string(kPort));
    log("request format: ISSUE|username|password|type");

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        HANDLE thread = CreateThread(nullptr, 0, client_thread, reinterpret_cast<LPVOID>(client), 0, nullptr);
        if (thread) CloseHandle(thread);
        else closesocket(client);
    }

    closesocket(server);
    WSACleanup();
    return 0;
}
