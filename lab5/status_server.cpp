#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

namespace {

constexpr int kPort = 9001;
constexpr int kBacklog = SOMAXCONN;
constexpr int kHeartbeatTimeoutSeconds = 90;

struct Session {
    std::string serial;
    std::string client_id;
    std::string peer;
    int limit = 10;
    int active = 1;
    std::chrono::steady_clock::time_point last_seen;
};

std::mutex g_mutex;
std::mutex g_log_mutex;
std::unordered_map<std::string, Session> g_sessions;

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

std::string peer_name(SOCKET s) {
    sockaddr_in addr{};
    int len = sizeof(addr);
    if (getpeername(s, reinterpret_cast<sockaddr*>(&addr), &len) == SOCKET_ERROR) {
        return "unknown";
    }
    const char* buf = inet_ntoa(addr.sin_addr);
    std::ostringstream oss;
    oss << buf << ":" << ntohs(addr.sin_port);
    return oss.str();
}

std::string make_key(const std::string& serial, const std::string& client_id) {
    return serial + "|" + client_id;
}

int license_limit_from_serial(const std::string& serial) {
    if (serial.size() == 10 && serial[0] >= '5') {
        return 50;
    }
    return 10;
}

void cleanup_thread() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(g_mutex);
        for (auto it = g_sessions.begin(); it != g_sessions.end();) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_seen).count();
            if (elapsed > kHeartbeatTimeoutSeconds) {
                log("timeout remove serial=" + it->second.serial + ", client=" + it->second.client_id + ", peer=" + it->second.peer);
                it = g_sessions.erase(it);
            } else {
                ++it;
            }
        }
    }
}

DWORD WINAPI client_thread(LPVOID param) {
    SOCKET client = reinterpret_cast<SOCKET>(param);
    const std::string peer = peer_name(client);
    std::string request;
    if (!recv_line(client, request)) {
        closesocket(client);
        return 0;
    }

    std::istringstream iss(request);
    std::string cmd, serial, client_id;
    std::getline(iss, cmd, '|');
    std::getline(iss, serial, '|');
    std::getline(iss, client_id, '|');

    cmd = trim(cmd);
    serial = trim(serial);
    client_id = trim(client_id);

    if (cmd != "AUTH" || serial.empty() || client_id.empty()) {
        send_line(client, "DENY|invalid_request");
        shutdown(client, SD_BOTH);
        closesocket(client);
        return 0;
    }

    const int announced_limit = license_limit_from_serial(serial);

    const std::string key = make_key(serial, client_id);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto now = std::chrono::steady_clock::now();
        auto& session = g_sessions[key];
        if (session.serial.empty()) {
            session.serial = serial;
            session.client_id = client_id;
            session.peer = peer;
            session.limit = announced_limit;
            session.active = 0;
        }
        if (session.active == 0) {
            int current = 0;
            for (const auto& [k, v] : g_sessions) {
                if (v.serial == serial && v.active > 0) {
                    ++current;
                }
            }
            if (current >= session.limit) {
                send_line(client, "DENY|license_limit_reached");
                log("deny serial=" + serial + ", client=" + client_id + ", peer=" + peer);
                shutdown(client, SD_BOTH);
                closesocket(client);
                return 0;
            }
            session.active = 1;
            log("allow serial=" + serial + ", client=" + client_id + ", peer=" + peer);
        } else {
            log("resume serial=" + serial + ", client=" + client_id + ", peer=" + peer);
        }
        session.last_seen = now;
    }

    if (!send_line(client, "ALLOW|30")) {
        closesocket(client);
        return 0;
    }

    while (true) {
        std::string line;
        if (!recv_line(client, line)) {
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_sessions.find(key);
            if (it != g_sessions.end()) {
                log("disconnect serial=" + serial + ", client=" + client_id + ", peer=" + peer);
                g_sessions.erase(it);
            }
            break;
        }

        std::istringstream ping_iss(line);
        std::string ping_cmd, ping_serial, ping_client_id;
        std::getline(ping_iss, ping_cmd, '|');
        std::getline(ping_iss, ping_serial, '|');
        std::getline(ping_iss, ping_client_id, '|');
        ping_cmd = trim(ping_cmd);
        ping_serial = trim(ping_serial);
        ping_client_id = trim(ping_client_id);

        if (ping_cmd == "PING" && ping_serial == serial && ping_client_id == client_id) {
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_sessions.find(key);
            if (it != g_sessions.end()) {
                it->second.last_seen = std::chrono::steady_clock::now();
            }
            send_line(client, "PONG");
        } else if (ping_cmd == "LOGOUT") {
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_sessions.find(key);
            if (it != g_sessions.end()) {
                log("logout serial=" + serial + ", client=" + client_id + ", peer=" + peer);
                g_sessions.erase(it);
            }
            break;
        } else {
            send_line(client, "ERR|bad_command");
        }
    }

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

    BOOL reuse = TRUE;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

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

    std::thread(cleanup_thread).detach();
    log("status server started on port " + std::to_string(kPort));
    log("request format: AUTH|serial|client_id then PING|serial|client_id");

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
