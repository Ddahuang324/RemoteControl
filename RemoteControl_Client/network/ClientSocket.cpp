// Precompiled header must be first for MSVC projects
#include "pch.h"
#include "clientSocket.h"
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>

static void ClientLog(const char* fmt) {
    try {
        std::ofstream ofs("client_socket_debug.log", std::ios::app);
        if (ofs) {
            ofs << fmt << std::endl;
        }
    } catch (...) {}
}

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
        ClientLog("[ClientSocket] socket() failed");
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
        ClientLog("[ClientSocket] inet_pton failed for IP: ");
        ClientLog(ip.c_str());
        return false;
    }

    if (connect(m_servSocket, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        wchar_t buf[128];
        swprintf_s(buf, _countof(buf), L"Connect failed: %d", WSAGetLastError());
        MessageBoxW(NULL, buf, L"Error", MB_OK);
        {
            std::ostringstream ss; ss << "[ClientSocket] connect() failed, WSAErr=" << WSAGetLastError();
            ClientLog(ss.str().c_str());
        }
        CloseSocket();
        return false;
    }

    // Prepare buffers and start receive loop
    m_recvBuffer.clear();
    m_running.store(true);
    m_recvThread = std::thread(&CclientSocket::RecvThreadLoop, this);
    {
        std::ostringstream ss; ss << "[ClientSocket] connected to " << ip << ":" << port;
        ClientLog(ss.str().c_str());
    }
    return true;
}

void CclientSocket::CloseSocket() {
    // Signal thread to stop
    ClientLog("[ClientSocket] CloseSocket called");
    m_running.store(false);
    if (m_servSocket != INVALID_SOCKET) {
        int rc = shutdown(m_servSocket, SD_BOTH);
        {
            std::ostringstream ss; ss << "[ClientSocket] shutdown returned=" << rc << ", WSAErr=" << WSAGetLastError();
            ClientLog(ss.str().c_str());
        }
        int cl = closesocket(m_servSocket);
        {
            std::ostringstream ss; ss << "[ClientSocket] closesocket returned=" << cl << ", WSAErr=" << WSAGetLastError();
            ClientLog(ss.str().c_str());
        }
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
    {
        std::ostringstream ss; ss << "[ClientSocket] SendPacket sent bytes=" << total << " cmd=" << packet.sCmd;
        ClientLog(ss.str().c_str());
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
            {
                std::ostringstream ss; ss << "[ClientSocket] recv returned bytes=" << ret;
                ClientLog(ss.str().c_str());
            }
            // append to m_recvBuffer (protected)
            {
                std::lock_guard<std::mutex> rb_lk(m_recvBufferMutex);
                m_recvBuffer.insert(m_recvBuffer.end(), tmp.data(), tmp.data() + ret);
            }

            // try to parse packets; coordinate via m_recvBufferMutex to allow safe clearing
            while (true) {
                // parse while holding recv buffer lock only for reading/erasing
                std::unique_lock<std::mutex> rb_lk(m_recvBufferMutex);
                size_t consumed = 0;
                auto opt = Cpacket::DeserializePacket(m_recvBuffer, consumed);
                if (!opt) {
                    if (consumed > 0) {
                        std::ostringstream ss; ss << "[ClientSocket] DeserializePacket incomplete/invalid, consumed=" << consumed;
                        ClientLog(ss.str().c_str());
                        // drop consumed bytes
                        if (consumed <= m_recvBuffer.size())
                            m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + consumed);
                        else
                            m_recvBuffer.clear();
                    } else {
                        ClientLog("[ClientSocket] DeserializePacket: need more data");
                    }
                    // release lock and wait for more data
                    rb_lk.unlock();
                    break;
                }

                // We have a complete packet. Move it out and remove consumed bytes while holding the recv lock.
                Cpacket pkt = std::move(*opt);
                if (consumed > 0) {
                    if (consumed <= m_recvBuffer.size())
                        m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + consumed);
                    else
                        m_recvBuffer.clear();
                }
                // release recv buffer lock before pushing into packet queue to avoid lock ordering deadlocks
                rb_lk.unlock();

                // push packet into queue
                {
                    std::ostringstream ss; ss << "[ClientSocket] Parsed packet cmd=" << pkt.sCmd << " dataSize=" << pkt.data.size();
                    ClientLog(ss.str().c_str());
                    std::lock_guard<std::mutex> lk(m_queueMutex);
                    m_packetQueue.push_back(std::move(pkt));
                    while (m_packetQueue.size() > m_maxQueue) m_packetQueue.pop_front();
                }
                m_queueCv.notify_one();
                // continue to parse more packets (will re-acquire rb_lk at loop top)
            }
        } else if (ret == 0) {
            // connection closed gracefully
            {
                std::ostringstream ss; ss << "[ClientSocket] recv returned 0 (peer closed)";
                ClientLog(ss.str().c_str());
            }
            m_running.store(false);
            break;
        } else {
            // error
            int err = WSAGetLastError();
            // If recoverable, continue; otherwise stop
            {
                std::ostringstream ss; ss << "[ClientSocket] recv error ret=" << ret << " WSAErr=" << err;
                ClientLog(ss.str().c_str());
            }
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

void CclientSocket::ClearPacketsByCmd(WORD cmd) {
    std::lock_guard<std::mutex> lk(m_queueMutex);
    if (m_packetQueue.empty()) return;
    std::deque<Cpacket> newq;
    newq.clear();
    for (auto &p : m_packetQueue) {
        if (p.sCmd != cmd) newq.push_back(std::move(p));
    }
    m_packetQueue.swap(newq);
}

void CclientSocket::ClearAllPackets() {
    std::lock_guard<std::mutex> lk(m_queueMutex);
    m_packetQueue.clear();
}

void CclientSocket::ClearRecvBuffer() {
    std::lock_guard<std::mutex> rb_lk(m_recvBufferMutex);
    m_recvBuffer.clear();
}
