#pragma once

#include <string>
#include <windows.h>
#include "Enities.h"
#include <filesystem>
#include "ServerSocket.h"
#include <iostream>
#include <fstream>




std::string GetDriverInfo() {
	DWORD driveMask = GetLogicalDrives();

	if(driveMask == 0) {
		return "";
	}
	std::string driveInfo;
	driveInfo.reserve(26 * 3); 
	
	for(int i  = 0; i < 26; ++i) {
		if(driveMask & (1 << i)) {
			char driveLetter = 'A' + i;
			driveInfo += driveLetter;
			driveInfo += ":\\ ";
		}
	}
	return driveInfo;
}

int DirectoryInfor(const std::string& path, CServerSocket& ClientSocket){
	std::filesystem::path targetPath(path);

	std::vector<File_Info> fileInfo;

	try{
		if (!std::filesystem::exists(targetPath) || !std::filesystem::is_directory(targetPath)) {
			
			std::cerr << "Invalid path: " << path << std::endl;
			std::string errMsg = "Invalid path: " + path;
			ClientSocket.SendErrorPacket(errMsg);
			return -1; // 错误
		}
		// 这里添加目录遍历逻辑
		for(const auto& entry : std::filesystem::directory_iterator(targetPath)){
			File_Info file;
			file.isDir = std::filesystem::is_directory(entry.path());
			file.fullPath = entry.path().string();
			fileInfo.push_back(file);
		}
		// 序列化文件信息
		std::vector<BYTE> serializedData;
		for(const auto& file : fileInfo){
			auto fileData = file.FileInfoSerialize();
			serializedData.insert(serializedData.end(), fileData.begin(), fileData.end());
		}
		// 发送序列化数据包
		Cpacket packet(CMD::CMD_DIRECTORY_INFO, serializedData);
		ClientSocket.SendPacket(packet);

		return 0; // 成功
	}
	catch (const std::filesystem::filesystem_error& e) {
		std::cerr << "Filesystem error: " << e.what() << std::endl;
		std::string errMsg = std::string("Filesystem error: ") + e.what();
		ClientSocket.SendErrorPacket(errMsg);
		return -1;
	}

}

int RunFile(const std::string& path, CServerSocket& ClientSocket) {
    // 运行文件
    HINSTANCE result = ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((int)result <= 32) {
        // 运行失败，发送错误包
        std::string errMsg = "Failed to run file: " + path;
        ClientSocket.SendErrorPacket(errMsg);
        return -1;
    }
    // 成功运行，发送确认包（可选）
    std::vector<BYTE> pathData(path.begin(), path.end());
    Cpacket packet(CMD::CMD_RUN_FILE, pathData);
    ClientSocket.SendPacket(packet);
    return 0;
}

int DownloadFile(const std::string& path, CServerSocket& ClientSocket) {
	
	std::ifstream ifile(path, std::ios::binary | std::ios::ate);

	if(!ifile.is_open()){

		std::cerr << "Failed to open file: " << path << std::endl;
		std::string errMsg = "Failed to open file: " + path;
		ClientSocket.SendErrorPacket(errMsg);
		return -1;
	}


	std::streamsize FileSize = ifile.tellg();
	ifile.seekg(0, std::ios::beg);

	// 发送文件大小
	std::vector<BYTE> sizeData(sizeof(std::streamsize));
	memcpy(sizeData.data(), &FileSize, sizeof(std::streamsize));
	Cpacket HeaderPacket(CMD::CMD_DOWNLOAD_FILE, sizeData);
	ClientSocket.SendPacket(HeaderPacket);

	std::vector<BYTE> buffer(1024);

	while (true) {
		ifile.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
		size_t bytesRead = ifile.gcount();
		if (bytesRead == 0) break;
		ClientSocket.SendPacket(Cpacket(CMD::CMD_DOWNLOAD_FILE, std::vector<BYTE>(buffer.data(), buffer.data() + bytesRead)));
	}

	// 发送结束包
	Cpacket eofPacket(CMD::CMD_EOF, std::vector<BYTE>());
	ClientSocket.SendPacket(eofPacket);

	return 0;
}



