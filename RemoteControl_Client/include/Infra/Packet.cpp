#include "pch.h"
#include "Packet.hpp"
#include <numeric>
#include <cstring>
#include <stdexcept>

// Constructors
Packet::Packet() : PacketBase() {}

Packet::Packet(WORD cmd, const std::vector<BYTE>& packetData)
    : PacketBase() {
    sHead = 0xFEFF;
    sCmd = cmd;
    data = packetData;
    sSum = 0;
    nLength = static_cast<DWORD>(sizeof(sCmd) + data.size() + sizeof(sSum));
}

Packet::Packet(WORD cmd, std::vector<BYTE>&& packetData)
    : PacketBase() {
    sHead = 0xFEFF;
    sCmd = cmd;
    data = std::move(packetData);
    sSum = 0;
    nLength = static_cast<DWORD>(sizeof(sCmd) + data.size() + sizeof(sSum));
}

// Deserialize implementation
std::optional<Packet> Packet::DeserializePacket(const std::vector<BYTE>& buffer, size_t& bytesconsumed) {
	bytesconsumed = 0;
	size_t bufferSize = buffer.size();

	// Step1: Find header (use memcpy to avoid alignment UB)
	size_t headpos = 0;
	bool headFound = false;
	for (; headpos + sizeof(WORD) <= bufferSize; ++headpos) {
		WORD val = 0;
		std::memcpy(&val, buffer.data() + headpos, sizeof(WORD));
		if (val == 0xFEFF) {
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

	const size_t minPacketSize = sizeof(WORD) + sizeof(DWORD) + sizeof(WORD) + sizeof(WORD);
	if (remainingSize < minPacketSize) {
		return std::nullopt;
	}

	Packet packet;
	size_t currentPos = headpos;

	// Step2: Extract header information
	std::memcpy(&packet.sHead, buffer.data() + currentPos, sizeof(packet.sHead));
	currentPos += sizeof(WORD);
	std::memcpy(&packet.nLength, buffer.data() + currentPos, sizeof(packet.nLength));
	currentPos += sizeof(DWORD);
	std::memcpy(&packet.sCmd, buffer.data() + currentPos, sizeof(packet.sCmd));
	currentPos += sizeof(WORD);

	// Validate declared length: must be at least size of cmd + checksum and not exceed MAX_PACKET_BODY
	if (packet.nLength < (sizeof(packet.sCmd) + sizeof(packet.sSum))) {
		bytesconsumed += sizeof(WORD);
		return std::nullopt;
	}

	if (packet.nLength > MAX_PACKET_BODY) {
		// Declared length too large — skip header byte and continue scanning
		bytesconsumed += sizeof(WORD);
		return std::nullopt;
	}

	DWORD dataLength = packet.nLength - sizeof(packet.sCmd) - sizeof(packet.sSum);

	// Step3: Check if data section is complete
	if ((currentPos - headpos) + dataLength + sizeof(packet.sSum) > remainingSize) {
		return std::nullopt;
	}

	if (dataLength > 0) {
		const auto dataStart = buffer.begin() + currentPos;
		const auto dataEnd = dataStart + dataLength;
		packet.data.assign(dataStart, dataEnd);
	}
	currentPos += dataLength;

	// Step4: Extract checksum
	std::memcpy(&packet.sSum, buffer.data() + currentPos, sizeof(packet.sSum));
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

// Serialize implementation
std::vector<BYTE> Packet::SerializePacket() const {
	WORD calculatedSum = 0;
	if (!data.empty()) {
		calculatedSum = std::accumulate(data.begin(), data.end(), static_cast<WORD>(0));
	}
	// Recompute body length (cmd + data + checksum) to avoid relying on possibly stale member `nLength`
	DWORD bodyLength = static_cast<DWORD>(sizeof(sCmd) + data.size() + sizeof(calculatedSum));

	// Reserve exact size and fill buffer to avoid multiple realloc/copies
	size_t totalSize = sizeof(sHead) + sizeof(nLength) + sizeof(sCmd) + data.size() + sizeof(calculatedSum);
	std::vector<BYTE> buffer;
	buffer.resize(totalSize);
	size_t pos = 0;
	std::memcpy(buffer.data() + pos, &sHead, sizeof(sHead)); pos += sizeof(sHead);
	std::memcpy(buffer.data() + pos, &bodyLength, sizeof(bodyLength)); pos += sizeof(bodyLength);
	std::memcpy(buffer.data() + pos, &sCmd, sizeof(sCmd)); pos += sizeof(sCmd);
	if (!data.empty()) {
		std::memcpy(buffer.data() + pos, data.data(), data.size()); pos += data.size();
	}
	std::memcpy(buffer.data() + pos, &calculatedSum, sizeof(calculatedSum)); pos += sizeof(calculatedSum);
	return buffer;
}

