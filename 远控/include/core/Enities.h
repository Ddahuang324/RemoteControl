#pragma once

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
	CMD_ERROR = 999
};


typedef struct File_Info {
	bool isDir;
	std::string fullPath;
	
	std::vector<BYTE> FileInfoSerialize() const {
		std::vector<BYTE> serializedData;
		// 序列化 isDir (1字节)
		serializedData.push_back(static_cast<BYTE>(isDir));
		// 序列化 fullPath 长度 (4字节, DWORD)
		DWORD pathLen = static_cast<DWORD>(fullPath.size());
		serializedData.insert(serializedData.end(), reinterpret_cast<BYTE*>(&pathLen), reinterpret_cast<BYTE*>(&pathLen) + sizeof(DWORD));
		// 序列化 fullPath 数据
		serializedData.insert(serializedData.end(), fullPath.begin(), fullPath.end());
		return serializedData;
	}

};


struct MouseEventData{
	
	WORD nAction;
	WORD nButton;
	POINT ptXY;


	static MouseEventData Deserialize(const std::vector<BYTE>& serializedData) {
		constexpr size_t expectedSize = sizeof(WORD) * 2 + sizeof(POINT);

		if (serializedData.size() < expectedSize) {
			throw std::runtime_error("Serialized data is too small to deserialize MouseEventData.");
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

