#pragma once

#include "Protocol/Infra/PacketProtocol.h"
#include "pch.h"
#include "framework.h"
#include <optional>
#include <cstddef>
#include <winsock2.h>
#include <ws2tcpip.h>

constexpr size_t MAX_PACKET_BODY = 10 * 1024 * 1024; // 10 MB

class Packet : public PacketBase {
public:
    Packet();
    Packet(WORD cmd, const std::vector<BYTE>& packetData);
    Packet(WORD cmd, std::vector<BYTE>&& packetData);

    static std::optional<Packet> DeserializePacket(const std::vector<BYTE>& buffer, size_t& bytesconsumed);

    std::vector<BYTE> SerializePacket() const;
};