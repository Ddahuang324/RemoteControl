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