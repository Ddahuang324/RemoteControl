#include "pch.h"
#include "network/ServerSocket.h"
#include "core/Enities.h"
#include <iostream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

class FakeClient {
public:
    FakeClient(const std::string& serverIP, int port)
        : serverIP(serverIP), port(port), sock(INVALID_SOCKET) {
        WSAData wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }

    ~FakeClient() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
        }
        WSACleanup();
    }

    bool connectToServer() {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            std::cerr << "Socket creation failed" << std::endl;
            return false;
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Connect failed with error: " << WSAGetLastError() << std::endl;
            closesocket(sock);
            sock = INVALID_SOCKET;
            return false;
        }

        std::cout << "Connected to server!" << std::endl;
        return true;
    }

    void sendPacket(const Cpacket& packet) {
        if (sock == INVALID_SOCKET) {
            std::cerr << "Not connected to server." << std::endl;
            return;
        }

        auto buffer = packet.SerializePacket();
        send(sock, reinterpret_cast<const char*>(buffer.data()), buffer.size(), 0);
    }

    void receiveLoop() {
        char buffer[4096];
        while (true) {
            int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
            if (bytesReceived > 0) {
                std::cout << "Received " << bytesReceived << " bytes from server." << std::endl;
            } else if (bytesReceived == 0) {
                std::cout << "Server closed the connection." << std::endl;
                break;
            } else {
                std::cerr << "Recv failed with error: " << WSAGetLastError() << std::endl;
                break;
            }
        }
    }

private:
    std::string serverIP;
    int port;
    SOCKET sock;
};

int main() {
    try {
        FakeClient client("127.0.0.1", 12345);
        if (!client.connectToServer()) {
            return 1;
        }

        // Start a separate thread to receive responses
        std::thread receiverThread(&FakeClient::receiveLoop, &client);
        receiverThread.detach();

        std::string line;
        while (true) {
            std::cout << "> ";
            std::getline(std::cin, line);
            if (line == "exit") {
                break;
            }

            if (line.empty()) {
                continue;
            }

            std::string command;
            std::string argument;
            size_t space_pos = line.find(' ');
            if (space_pos != std::string::npos) {
                command = line.substr(0, space_pos);
                argument = line.substr(space_pos + 1);
            } else {
                command = line;
            }

            if (command == "help") {
                std::cout << "--- Fake Client Commands ---\n"
                          << "help           - Show this help message\n"
                          << "exit           - Exit the client\n"
                          << "driver_info    - Request driver information\n"
                          << "dir_info <path> - Request directory information\n"
                          << "run_file <path> - Request to run a file\n"
                          << "download_file <path> - Request to download a file\n"
                          << "lock_machine   - Lock the machine\n"
                          << "unlock_machine - Unlock the machine\n"
                          << "--------------------------\n";
            } else if (command == "driver_info") {
                Cpacket packet(CMD::CMD_DRIVER_INFO, {});
                client.sendPacket(packet);
            } else if (command == "dir_info") {
                if (argument.empty()) {
                    std::cout << "Usage: dir_info <path>\n";
                } else {
                    std::vector<BYTE> data(argument.begin(), argument.end());
                    Cpacket packet(CMD::CMD_DIRECTORY_INFO, data);
                    client.sendPacket(packet);
                }
            } else if (command == "run_file") {
                if (argument.empty()) {
                    std::cout << "Usage: run_file <path>\n";
                } else {
                    std::vector<BYTE> data(argument.begin(), argument.end());
                    Cpacket packet(CMD::CMD_RUN_FILE, data);
                    client.sendPacket(packet);
                }
            } else if (command == "download_file") {
                if (argument.empty()) {
                    std::cout << "Usage: download_file <path>\n";
                } else {
                    std::vector<BYTE> data(argument.begin(), argument.end());
                    Cpacket packet(CMD::CMD_DOWNLOAD_FILE, data);
                    client.sendPacket(packet);
                }
            } else if (command == "lock_machine") {
                Cpacket packet(CMD::CMD_LOCK_MACHINE, {});
                client.sendPacket(packet);
            } else if (command == "unlock_machine") {
                Cpacket packet(CMD::CMD_UNLOCK_MACHINE, {});
                client.sendPacket(packet);
            } else {
                std::cout << "Unknown command: " << command << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
