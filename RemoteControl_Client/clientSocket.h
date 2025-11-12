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

#pragma comment(lib, "ws2_32.lib")

using BYTE = unsigned char;
using WORD = unsigned short;
using DWORD = unsigned long;

// --- 1. Copied from ServerSocket.h for Cpacket and WSAInitializer ---
// (Ensure client and server use the same definitions)

class Cpacket {
public:
    Cpacket() : sHead(0), nLength(0), sCmd(0), sSum(0) {}

    Cpacket(WORD cmd, const std::vector<BYTE>& packetData) : sHead(0xFEFF), sCmd(cmd), data(packetData), sSum(0) {
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

        if (packet.nLength < (sizeof(sCmd) + sizeof(sSum))) {
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
        std::vector<BYTE> buffer;
        WORD calculatedSum = 0;
        if (!data.empty()) {
            calculatedSum = std::accumulate(data.begin(), data.end(), static_cast<WORD>(0));
        }
        buffer.insert(buffer.end(), reinterpret_cast<const BYTE*>(&sHead), reinterpret_cast<const BYTE*>(&sHead) + sizeof(sHead));
        buffer.insert(buffer.end(), reinterpret_cast<const BYTE*>(&nLength), reinterpret_cast<const BYTE*>(&nLength) + sizeof(nLength));
        buffer.insert(buffer.end(), reinterpret_cast<const BYTE*>(&sCmd), reinterpret_cast<const BYTE*>(&sCmd) + sizeof(sCmd));
        buffer.insert(buffer.end(), data.begin(), data.end());
        buffer.insert(buffer.end(), reinterpret_cast<const BYTE*>(&calculatedSum), reinterpret_cast<const BYTE*>(&calculatedSum) + sizeof(calculatedSum));
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
};
