#include "pch.h"
#include "clientSocket.h"
#include <array>


// Note: BUFFER_SIZE is already defined in the class as static constexpr (inline definition in C++17), no need to define here.

CclientSocket::CclientSocket() {
    // m_wsaInit is automatically initialized in constructor
}


CclientSocket::~CclientSocket() {
    CloseSocket();
}

bool CclientSocket::connectToServer(const std::string& ip, unsigned short port) {
    // If connection exists, close first
    CloseSocket();

    m_servSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_servSocket == INVALID_SOCKET) {
        MessageBoxW(NULL, L"Socket creation failed", L"Error", MB_OK);
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) != 1) {
        MessageBoxW(NULL, L"Invalid IP address", L"Error", MB_OK);
        CloseSocket();
        return false;
    }

    if (connect(m_servSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        MessageBoxW(NULL, L"Connect failed", L"Error", MB_OK);
        CloseSocket();
        return false;
    }

    // Clear receive buffer to prevent old data interference
    m_recvBuffer.clear();
    return true;
}

void CclientSocket::CloseSocket() {
    if (m_servSocket != INVALID_SOCKET) {
        closesocket(m_servSocket);
        m_servSocket = INVALID_SOCKET;
    }
}

bool CclientSocket::SendPacket(const Cpacket& packet) {
    if (m_servSocket == INVALID_SOCKET) {
        return false;
    }
    auto buffer = packet.SerializePacket();
    int result = send(m_servSocket, reinterpret_cast<const char*>(buffer.data()), buffer.size(), 0);
    return result != SOCKET_ERROR;
}

std::optional<Cpacket> CclientSocket::RecvPacket() {
    if (m_servSocket == INVALID_SOCKET) {
        return std::nullopt;
    }

    while (true) {
        size_t bytesConsumed = 0;
        std::optional<Cpacket> packetOpt = Cpacket::DeserializePacket(m_recvBuffer, bytesConsumed);

        if (packetOpt.has_value()) {
            m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + bytesConsumed);
            return packetOpt;
        }

        if (bytesConsumed > 0) {
            m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + bytesConsumed);
        }

        std::array<BYTE, BUFFER_SIZE> tempBuffer{};
        int bytesReceived = recv(m_servSocket, reinterpret_cast<char*>(tempBuffer.data()), static_cast<int>(BUFFER_SIZE), 0);

        if (bytesReceived <= 0) {
            CloseSocket();
            return std::nullopt;
        }

        m_recvBuffer.insert(m_recvBuffer.end(), tempBuffer.begin(), tempBuffer.begin() + bytesReceived);
    }
}
