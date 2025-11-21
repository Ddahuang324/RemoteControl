#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>


using BYTE = unsigned char;
using DWORD = unsigned long;

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

typedef struct File_Info {
  bool hasNext;
  bool isDir;
  std::string fullPath;

  File_Info() = default;
  File_Info(bool isDir, const std::string &fullPath, bool hasNext)
      : isDir(isDir), fullPath(fullPath), hasNext(hasNext) {}
  std::vector<BYTE> FileInfoSerialize() const {
    std::vector<BYTE> serializedData;
    // 序列化 isDir (1字节)
    serializedData.push_back(static_cast<BYTE>(isDir));
    // 序列化 fullPath 长度 (4字节, DWORD)
    DWORD pathLen = static_cast<DWORD>(fullPath.size());
    serializedData.insert(serializedData.end(),
                          reinterpret_cast<BYTE *>(&pathLen),
                          reinterpret_cast<BYTE *>(&pathLen) + sizeof(DWORD));
    // 序列化 fullPath 数据
    serializedData.insert(serializedData.end(), fullPath.begin(),
                          fullPath.end());
    return serializedData;
  }

  static std::optional<File_Info>
  Deserialize(const std::vector<BYTE> &serializedData) {
    if (serializedData.empty())
      return std::nullopt;

    try {
      File_Info file;
      size_t offset = 0;

      if (offset + sizeof(BYTE) > serializedData.size())
        return std::nullopt;
      file.isDir = static_cast<bool>(serializedData[offset]);
      offset += sizeof(BYTE);

      if (offset + sizeof(DWORD) > serializedData.size())
        return std::nullopt;
      DWORD pathLen =
          *reinterpret_cast<const DWORD *>(serializedData.data() + offset);
      offset += sizeof(DWORD);

      if (offset + pathLen > serializedData.size())
        return std::nullopt;
      file.fullPath = std::string(
          reinterpret_cast<const char *>(serializedData.data() + offset),
          pathLen);
      offset += pathLen;

      if (offset + sizeof(BYTE) > serializedData.size())
        return std::nullopt;
      file.hasNext = static_cast<bool>(serializedData[offset]);
      offset += sizeof(BYTE);

      return file;
    } catch (...) {
      return std::nullopt;
    }
  }
};

struct MouseEventData {

  WORD nAction;
  WORD nButton;
  POINT ptXY;

  static MouseEventData Deserialize(const std::vector<BYTE> &serializedData) {
    constexpr size_t expectedSize = sizeof(WORD) * 2 + sizeof(POINT);

    if (serializedData.size() < expectedSize) {
      throw std::runtime_error(
          "Serialized data is too small to deserialize MouseEventData.");
    }

    MouseEventData eventData;
    size_t offset = 0;

    memcpy(&eventData.nAction, serializedData.data() + offset, sizeof(WORD));
    offset += sizeof(WORD);

    memcpy(&eventData.nButton, serializedData.data() + offset, sizeof(WORD));
    offset += sizeof(WORD);

    memcpy(&eventData.ptXY, serializedData.data() + offset, sizeof(POINT));
    offset += sizeof(POINT);

    return eventData;
  }

  std::vector<BYTE> Serialize() const {
    constexpr size_t dataSize = sizeof(WORD) * 2 + sizeof(POINT);
    std::vector<BYTE> serializedData(dataSize);
    size_t offset = 0;

    memcpy(serializedData.data() + offset, &nAction, sizeof(WORD));
    offset += sizeof(WORD);

    memcpy(serializedData.data() + offset, &nButton, sizeof(WORD));
    offset += sizeof(WORD);

    memcpy(serializedData.data() + offset, &ptXY, sizeof(POINT));
    offset += sizeof(POINT);

    return serializedData;
  }
};
