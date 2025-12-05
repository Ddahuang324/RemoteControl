#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <mutex>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")

// RAII wrapper for WSA initialization
class WSAInitializer {
public:
    WSAInitializer();
    ~WSAInitializer();
};

// Base structure for socket protocol
struct SocketBase {
    WSAInitializer wsaInit; // RAII for WSA
    SOCKET servSocket = INVALID_SOCKET; // 原始 socket 句柄
    std::vector<uint8_t> recvBuffer; // TCP byte stream receive buffer
    std::mutex recvBufferMutex;     // Mutex to protect recvBuffer (concurrent access)

    SocketBase() = default;
    ~SocketBase() {
        if (servSocket != INVALID_SOCKET) {
            closesocket(servSocket);
        }
    }
};