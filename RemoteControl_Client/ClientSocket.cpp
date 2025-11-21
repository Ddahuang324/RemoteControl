// Precompiled header must be first for MSVC projects
#include "pch.h"
#include "clientSocket.h"
#include <array>
#include <chrono>
#include <cstring>

CclientSocket::CclientSocket() {
    // m_wsaInit RAII will initialize Winsock
    m_recvBuffer.reserve(BUFFER_SIZE * 4);
}

CclientSocket::~CclientSocket() {
    CloseSocket();
}

bool CclientSocket::connectToServer(const std::string &ip, unsigned short port) {
    // If connection exists, close first
    CloseSocket();

    m_servSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_servSocket == INVALID_SOCKET) {
        MessageBoxW(NULL, L"Socket creation failed", L"Error", MB_OK);
        return false;
    }

    // Set socket options
    int flag = 1;
    setsockopt(m_servSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&flag), sizeof(flag));
    int rcv = 256 * 1024;
    setsockopt(m_servSocket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&rcv), sizeof(rcv));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) != 1) {
        MessageBoxW(NULL, L"Invalid IP address", L"Error", MB_OK);
        CloseSocket();
        return false;
    }

    if (connect(m_servSocket, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        MessageBoxW(NULL, L"Connect failed", L"Error", MB_OK);
        CloseSocket();
        return false;
    }

    // Prepare buffers and start receive loop
    m_recvBuffer.clear();
    m_running.store(true);
    m_recvThread = std::thread(&CclientSocket::RecvThreadLoop, this);
    return true;
}

void CclientSocket::CloseSocket() {
    // Signal thread to stop
    m_running.store(false);
    if (m_servSocket != INVALID_SOCKET) {
        shutdown(m_servSocket, SD_BOTH);
        closesocket(m_servSocket);
        m_servSocket = INVALID_SOCKET;
    }
    // wake any waiting threads
    m_queueCv.notify_all();
    if (m_recvThread.joinable()) {
        m_recvThread.join();
    }
    // clear queued packets
    {
        std::lock_guard<std::mutex> lk(m_queueMutex);
        m_packetQueue.clear();
    }
}

bool CclientSocket::SendPacket(const Cpacket &packet) {
    if (m_servSocket == INVALID_SOCKET) return false;
    std::vector<BYTE> buf = packet.SerializePacket();
    int total = 0;
    int toSend = (int)buf.size();
    const char *data = reinterpret_cast<const char *>(buf.data());
    while (total < toSend) {
        int ret = send(m_servSocket, data + total, toSend - total, 0);
        if (ret == SOCKET_ERROR) return false;
        total += ret;
    }
    return true;
}

std::optional<Cpacket> CclientSocket::RecvPacket() {
    // non-blocking attempt to get next packet
    return GetNextPacketBlocking(0);
}

void CclientSocket::RecvThreadLoop() {
    std::array<char, BUFFER_SIZE> tmp{};
    while (m_running.load()) {
        int ret = recv(m_servSocket, tmp.data(), (int)tmp.size(), 0);
        if (ret > 0) {
            // append to m_recvBuffer
            m_recvBuffer.insert(m_recvBuffer.end(), tmp.data(), tmp.data() + ret);

            // try to parse packets
            while (true) {
                size_t consumed = 0;
                auto opt = Cpacket::DeserializePacket(m_recvBuffer, consumed);
                if (!opt) {
                    // No complete packet yet, but we can drop consumed bytes
                    if (consumed > 0) {
                        m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + consumed);
                    }
                    break;
                }
                // push packet into queue
                {
                    std::lock_guard<std::mutex> lk(m_queueMutex);
                    m_packetQueue.push_back(std::move(*opt));
                    while (m_packetQueue.size() > m_maxQueue) m_packetQueue.pop_front();
                }
                m_queueCv.notify_one();
                // remove consumed bytes
                if (consumed > 0) {
                    if (consumed <= m_recvBuffer.size())
                        m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + consumed);
                    else
                        m_recvBuffer.clear();
                }
            }
        } else if (ret == 0) {
            // connection closed gracefully
            m_running.store(false);
            break;
        } else {
            // error
            int err = WSAGetLastError();
            // If recoverable, continue; otherwise stop
            m_running.store(false);
            break;
        }
    }
    // ensure any waiting consumers are released
    m_queueCv.notify_all();
}

std::optional<Cpacket> CclientSocket::GetLatestPacket() {
    std::lock_guard<std::mutex> lk(m_queueMutex);
    if (m_packetQueue.empty()) return std::nullopt;
    Cpacket latest = std::move(m_packetQueue.back());
    m_packetQueue.clear();
    return latest;
}

std::optional<Cpacket> CclientSocket::GetNextPacketBlocking(int timeoutMs) {
    std::unique_lock<std::mutex> lk(m_queueMutex);
    if (timeoutMs == 0) {
        m_queueCv.wait(lk, [this] { return !m_packetQueue.empty() || !m_running.load(); });
    } else {
        if (!m_queueCv.wait_for(lk, std::chrono::milliseconds(timeoutMs), [this] { return !m_packetQueue.empty() || !m_running.load(); })) {
            return std::nullopt; // timeout
        }
    }

    if (m_packetQueue.empty()) return std::nullopt;
    Cpacket pkt = std::move(m_packetQueue.front());
    m_packetQueue.pop_front();
    return pkt;
}
