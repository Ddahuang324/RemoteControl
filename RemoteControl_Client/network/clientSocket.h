#pragma once

#include "pch.h"
#include "framework.h"
#include <string>
#include <vector>
#include <optional>     
#include <numeric>      // for std::accumulate
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

using BYTE = unsigned char;
using WORD = unsigned short;
using DWORD = unsigned long;

// Maximum allowed single packet body (data + cmd + checksum). Protect against huge/poisoned length fields.
constexpr size_t MAX_PACKET_BODY = 10 * 1024 * 1024; // 10 MB

// --- 1. Copied from ServerSocket.h for Cpacket and WSAInitializer ---
// (Ensure client and server use the same definitions)

class Cpacket {
public:
    Cpacket() : sHead(0), nLength(0), sCmd(0), sSum(0) {}

    Cpacket(WORD cmd, const std::vector<BYTE>& packetData) : sHead(0xFEFF), sCmd(cmd), data(packetData), sSum(0) {
        nLength = sizeof(sCmd) + data.size() + sizeof(sSum);
    }
    // Move-constructor for packet data to avoid copy when possible
    Cpacket(WORD cmd, std::vector<BYTE>&& packetData) : sHead(0xFEFF), sCmd(cmd), data(std::move(packetData)), sSum(0) {
        nLength = sizeof(sCmd) + data.size() + sizeof(sSum);
    }
    WORD sHead; // Packet header, fixed to 0xFEFF
    DWORD nLength; // Packet body length (from command word to checksum end)
    WORD sCmd; // Command word
    std::vector<BYTE> data; // Data
    WORD sSum; // Checksum

    // Parse a complete Cpacket from buffer; bytesconsumed indicates consumed bytes
    static std::optional<Cpacket> DeserializePacket(const std::vector<BYTE>& buffer, size_t& bytesconsumed) {
        bytesconsumed = 0;
        size_t bufferSize = buffer.size();

        // Step1: Find header
        size_t headpos = 0;
        bool headFound = false;
        for (; headpos + sizeof(WORD) <= bufferSize; ++headpos) {
            if (*reinterpret_cast<const WORD*>(&buffer[headpos]) == 0xFEFF) {
                headFound = true;
                break;
            }
        }

        if (!headFound) {
            bytesconsumed = (bufferSize > 0) ? bufferSize - 1 : 0;
            return std::nullopt;
        }

        bytesconsumed = headpos;
        size_t remainingSize = bufferSize - headpos;

        const size_t minPacketSize = sizeof(sHead) + sizeof(nLength) + sizeof(sCmd) + sizeof(sSum);
        if (remainingSize < minPacketSize) {
            return std::nullopt;
        }

        Cpacket packet;
        size_t currentPos = headpos;

        // Step2: Extract header information
        packet.sHead = *reinterpret_cast<const WORD*>(&buffer[currentPos]);
        currentPos += sizeof(WORD);
        packet.nLength = *reinterpret_cast<const DWORD*>(&buffer[currentPos]);
        currentPos += sizeof(DWORD);
        packet.sCmd = *reinterpret_cast<const WORD*>(&buffer[currentPos]);
        currentPos += sizeof(WORD);

        // Validate declared length: must be at least size of cmd + checksum and not exceed MAX_PACKET_BODY
        if (packet.nLength < (sizeof(sCmd) + sizeof(sSum))) {
            bytesconsumed += sizeof(WORD);
            return std::nullopt;
        }

        if (packet.nLength > MAX_PACKET_BODY) {
            // Declared length too large — skip header byte and continue scanning
            bytesconsumed += sizeof(WORD);
            return std::nullopt;
        }

        DWORD dataLength = packet.nLength - sizeof(sCmd) - sizeof(sSum);

        // Step3: Check if data section is complete
        if ((currentPos - headpos) + dataLength + sizeof(sSum) > remainingSize) {
            return std::nullopt;
        }

        if (dataLength > 0) {
            const auto dataStart = buffer.begin() + currentPos;
            const auto dataEnd = dataStart + dataLength;
            packet.data.assign(dataStart, dataEnd);
        }
        currentPos += dataLength;

        // Step4: Extract checksum
        packet.sSum = *reinterpret_cast<const WORD*>(&buffer[currentPos]);
        currentPos += sizeof(WORD);

        // Step5: Verify checksum
        WORD calculatedSum = 0;
        if (!packet.data.empty()) {
            calculatedSum = std::accumulate(packet.data.begin(), packet.data.end(), static_cast<WORD>(0));
        }
        if (calculatedSum != packet.sSum) {
            bytesconsumed += sizeof(WORD);
            return std::nullopt;
        }

        bytesconsumed = currentPos;
        return packet;
    }

    // Serialize current Cpacket to byte stream
    std::vector<BYTE> SerializePacket() const {
        WORD calculatedSum = 0;
        if (!data.empty()) {
            calculatedSum = std::accumulate(data.begin(), data.end(), static_cast<WORD>(0));
        }

        // Reserve exact size and fill buffer to avoid multiple realloc/copies
        size_t totalSize = sizeof(sHead) + sizeof(nLength) + sizeof(sCmd) + data.size() + sizeof(calculatedSum);
        std::vector<BYTE> buffer;
        buffer.resize(totalSize);
        size_t pos = 0;
        memcpy(buffer.data() + pos, &sHead, sizeof(sHead)); pos += sizeof(sHead);
        memcpy(buffer.data() + pos, &nLength, sizeof(nLength)); pos += sizeof(nLength);
        memcpy(buffer.data() + pos, &sCmd, sizeof(sCmd)); pos += sizeof(sCmd);
        if (!data.empty()) {
            memcpy(buffer.data() + pos, data.data(), data.size()); pos += data.size();
        }
        memcpy(buffer.data() + pos, &calculatedSum, sizeof(calculatedSum)); pos += sizeof(calculatedSum);
        return buffer;
    }
};

class WSAInitializer {
public:
    WSAInitializer() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            MessageBoxW(NULL, L"WSAStartup failed", L"Error", MB_OK);
            throw std::runtime_error("WSAStartup failed");
        }
    }
    ~WSAInitializer() {
        WSACleanup();
    }
};


class CclientSocket
{
public:
    CclientSocket();
    ~CclientSocket();

    // Disable copy and assignment
    CclientSocket(const CclientSocket&) = delete;
    CclientSocket& operator=(const CclientSocket&) = delete;

    bool connectToServer(const std::string& ip, unsigned short port);
    void CloseSocket();
    bool SendPacket(const Cpacket& packet);
    std::optional<Cpacket> RecvPacket(); // Similar to server's RecvPacket

private:
    WSAInitializer m_wsaInit; // RAII
    SOCKET m_servSocket = INVALID_SOCKET;
    std::vector<BYTE> m_recvBuffer; // Receive buffer for handling TCP stream
    static constexpr size_t BUFFER_SIZE = 4096;
    static constexpr size_t MAX_TOTAL_BUFFER = 20 * 1024 * 1024; // 20 MB total buffered data cap
    // Threading + queue for decoupling network receive and UI rendering
    std::thread m_recvThread;
    std::mutex m_recvBufferMutex;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::deque<Cpacket> m_packetQueue;
    std::atomic_bool m_running{false};
    size_t m_maxQueue = 3; // keep only latest few frames

private:
    void RecvThreadLoop();

public:
    // Non-blocking retrieval of the latest received packet (drops older frames)
    std::optional<Cpacket> GetLatestPacket();
    // Blocking retrieval of the next available packet from the receive queue.
    // timeoutMs: maximum wait in milliseconds (0 = wait indefinitely). Returns nullopt on timeout or socket closed.
    // timeoutMs: maximum wait in milliseconds (0 = wait indefinitely).
    // 默认稍微增大到15秒，排查网络延时/竞态问题时更稳妥。
    std::optional<Cpacket> GetNextPacketBlocking(int timeoutMs = 15000);
    // 清空底层接收缓冲（未解析的 TCP 字节），线程安全
    void ClearRecvBuffer();
    // 清理接收队列中指定命令的包（线程安全）
    void ClearPacketsByCmd(WORD cmd);
    // 清空所有已缓存的包（线程安全）
    void ClearAllPackets();
};
