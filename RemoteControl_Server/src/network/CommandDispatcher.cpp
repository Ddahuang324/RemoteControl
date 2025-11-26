#include "pch.h"
#include "CommandDispatcher.h"
#include "fileSystem.h"
#include "InputeSimulator.h"
#include "screenCapture.h"
#include "LockMachine.h"
#include <iostream>

CommandDispatcher::CommandDispatcher()
{
    handlers_[static_cast<int>(CMD::CMD_DRIVER_INFO)] = [](const Cpacket&, CServerSocket& serverSocket) {
        std::string driveInfo = GetDriverInfo();
        std::vector<BYTE> data(driveInfo.begin(), driveInfo.end());
        Cpacket response(CMD::CMD_DRIVER_INFO, data);
        serverSocket.SendPacket(response);
    };

    handlers_[static_cast<int>(CMD::CMD_DIRECTORY_INFO)] = [](const Cpacket& packet, CServerSocket& serverSocket) {
        if (!packet.data.empty()) {
            std::string path(packet.data.begin(), packet.data.end());
            DirectoryInfor(path, serverSocket);
        }
    };

    handlers_[static_cast<int>(CMD::CMD_RUN_FILE)] = [](const Cpacket& packet, CServerSocket& serverSocket) {
        if (!packet.data.empty()) {
            std::string path(packet.data.begin(), packet.data.end());
            RunFile(path, serverSocket);
        }
    };

    handlers_[static_cast<int>(CMD::CMD_DOWNLOAD_FILE)] = [](const Cpacket& packet, CServerSocket& serverSocket) {
        if (!packet.data.empty()) {
            std::string path(packet.data.begin(), packet.data.end());
            DownloadFile(path, serverSocket);
        }
    };

    handlers_[static_cast<int>(CMD::CMD_DELETE_FILE)] = [](const Cpacket& packet, CServerSocket& serverSocket) {
        if (!packet.data.empty()) {
            std::string path(packet.data.begin(), packet.data.end());
            DeleteFile(path, serverSocket);
        }
    };

    handlers_[static_cast<int>(CMD::CMD_MOUSE_EVENT)] = [](const Cpacket& packet, CServerSocket& serverSocket) {
        HandleMouseEvent(serverSocket, packet);
    };

    handlers_[static_cast<int>(CMD::CMD_SCREEN_CAPTURE)] = [](const Cpacket& packet, CServerSocket& serverSocket) {
        CaptureScreen(serverSocket, packet);
    };

    handlers_[static_cast<int>(CMD::CMD_LOCK_MACHINE)] = [](const Cpacket& packet, CServerSocket& serverSocket) {
        LockMachine(serverSocket, packet);
    };

    handlers_[static_cast<int>(CMD::CMD_UNLOCK_MACHINE)] = [](const Cpacket& packet, CServerSocket& serverSocket) {
        UnlockMachine(serverSocket, packet);
    };

    handlers_[static_cast<int>(CMD::CMD_TEST_CONNECT)] = [](const Cpacket&, CServerSocket& serverSocket) {
        std::wcout << L"Test Connect received. Sending ACK." << std::endl;
        Cpacket response(CMD::CMD_TEST_CONNECT, {});
        serverSocket.SendPacket(response);
    };
}

void CommandDispatcher::Dispatch(const Cpacket& packet, CServerSocket& serverSocket) const
{
    std::wcout << L"Received packet - Cmd: " << packet.sCmd
               << L", Length: " << packet.nLength
               << L", Data Size: " << packet.data.size()
               << L", Checksum: " << packet.sSum << std::endl;

    auto it = handlers_.find(static_cast<int>(packet.sCmd));
    if (it != handlers_.end()) {
        it->second(packet, serverSocket);
    }
    else {
        std::string errMsg = "Unknown command";
        serverSocket.SendErrorPacket(errMsg);
    }
}
