#include "platform/net_hal.hpp"

#include <cstdio>
#include <cstring>

// Платформенный слой: единственное место, где допустим #ifdef под ОС
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
namespace {
using SockT = SOCKET;
constexpr SockT kInvalidSock = INVALID_SOCKET;
void close_sock(SockT s) { closesocket(s); }
bool set_nonblocking(SockT s) {
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
}
bool send_would_block() { return WSAGetLastError() == WSAEWOULDBLOCK; }
constexpr int kSendFlags = 0;
} // namespace
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
namespace {
using SockT = int;
constexpr SockT kInvalidSock = -1;
void close_sock(SockT s) { ::close(s); }
bool set_nonblocking(SockT s) {
    return fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK) == 0;
}
bool send_would_block() { return errno == EWOULDBLOCK || errno == EAGAIN; }
#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL; // без SIGPIPE при разрыве
#else
constexpr int kSendFlags = 0;
#endif
} // namespace
#endif

namespace {
SockT g_listen = kInvalidSock;
SockT g_client = kInvalidSock;
} // namespace

namespace platform {

bool net_listen(uint16_t port) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
#endif
    g_listen = ::socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen == kInvalidSock) {
        return false;
    }
    const int yes = 1;
    ::setsockopt(g_listen, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // только локальный дашборд

    if (::bind(g_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        ::listen(g_listen, 1) != 0 || !set_nonblocking(g_listen)) {
        close_sock(g_listen);
        g_listen = kInvalidSock;
        return false;
    }
    return true;
}

void net_poll() {
    if (g_listen == kInvalidSock) {
        return;
    }
    const SockT incoming = ::accept(g_listen, nullptr, nullptr);
    if (incoming != kInvalidSock) {
        if (g_client != kInvalidSock) {
            close_sock(g_client); // новый дашборд вытесняет прежнего
        }
        g_client = incoming;
        set_nonblocking(g_client);
    }
}

bool net_send_line(const char* line) {
    if (g_client == kInvalidSock) {
        return false;
    }
    char buf[256];
    const int len = std::snprintf(buf, sizeof(buf), "%s\n", line);
    if (len <= 0 || len >= static_cast<int>(sizeof(buf))) {
        return false;
    }
    const int sent = static_cast<int>(::send(g_client, buf, len, kSendFlags));
    if (sent == len) {
        return true;
    }
    if (sent < 0 && send_would_block()) {
        return false; // клиент не успевает -- телеметрию теряем, не копим
    }
    close_sock(g_client); // разрыв соединения
    g_client = kInvalidSock;
    return false;
}

void net_shutdown() {
    if (g_client != kInvalidSock) {
        close_sock(g_client);
        g_client = kInvalidSock;
    }
    if (g_listen != kInvalidSock) {
        close_sock(g_listen);
        g_listen = kInvalidSock;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

} // namespace platform
