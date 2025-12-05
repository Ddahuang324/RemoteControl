#pragma once

#include <vector>
#include <cstdint>

// Base structure for packet protocol
struct PacketBase {
    uint16_t sHead; // Packet header, fixed to 0xFEFF
    uint32_t nLength; // Packet body length (from command word to checksum end)
    uint16_t sCmd; // Command word
    uint16_t sSum; // Checksum
    std::vector<uint8_t> data; // Data payload

    PacketBase() : sHead(0xFEFF), nLength(0), sCmd(0), sSum(0), data() {}
};

enum CMD : unsigned short {
  CMD_DRIVER_INFO = 1,
  CMD_DIRECTORY_INFO = 2,
  CMD_RUN_FILE = 3,
  CMD_DOWNLOAD_FILE = 4,
  CMD_EOF = 5,
  CMD_MOUSE_EVENT = 6,
  CMD_SCREEN_CAPTURE = 7,
  CMD_SCREEN_DIFF = 8,
  CMD_LOCK_MACHINE = 9,
  CMD_UNLOCK_MACHINE = 10,
  CMD_DELETE_FILE = 11,
  CMD_TEST_CONNECT = 2022,
  CMD_ERROR = 999
};