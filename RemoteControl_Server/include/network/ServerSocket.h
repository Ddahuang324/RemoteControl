#pragma once
#include "../pch/framework.h"
#include "Enities.h"
#include <numeric>
#include <optional>
#include <string>
#include <vector>
#include <functional>
#include <winsock2.h>
#include <iostream>
#include <iomanip>

using BYTE = unsigned char;
using WORD = unsigned short;
using DWORD = unsigned long;

class Cpacket {
public:
	Cpacket() : sHead(0), nLength(0), sCmd(0), sSum(0) {}  // 初始化避免警告

	Cpacket(WORD cmd, const std::vector<BYTE>& packetData) : sHead(0xFEFF), sCmd(cmd), data(packetData), sSum(0) {
		nLength = sizeof(sCmd) + data.size() + sizeof(sSum);
	}
	WORD sHead;//包头,固定为FEFF
	DWORD nLength;//包体长度(从命令字开始到校验和结束)
	WORD sCmd;//命令字
	std::vector<BYTE> data;//数据
	WORD sSum;//校验和

	// buffer中解析一个完整的 Cpacket 并返回；bytesconsumed 表示消费的字节数
    static std::optional <Cpacket> DeserializePacket(const std::vector<BYTE>& buffer, size_t& bytesconsumed) {
		bytesconsumed = 0;
		size_t bufferSize = buffer.size();

        // Step1: 查找包头
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

		// Step2: 读取头、长度、命令
		packet.sHead = *reinterpret_cast<const WORD*>(&buffer[currentPos]);
		currentPos += sizeof(WORD);
		packet.nLength = *reinterpret_cast<const DWORD*>(&buffer[currentPos]);
		currentPos += sizeof(DWORD);
		packet.sCmd = *reinterpret_cast<const WORD*>(&buffer[currentPos]);
		currentPos += sizeof(WORD);

        if(packet.nLength < (sizeof(sCmd) + sizeof(sSum)) ) {
            bytesconsumed += sizeof(WORD);
            return std::nullopt; 
		}

		DWORD dataLength = packet.nLength - sizeof(sCmd) - sizeof(sSum);

		// Step3: 检查缓冲区是否包含完整包
        if ((currentPos - headpos) + dataLength + sizeof(sSum) > remainingSize) {
			return std::nullopt; 
        }
        
		if (dataLength > 0) {
			const auto dataStart = buffer.begin() + currentPos;
			const auto dataEnd = dataStart + dataLength;
			packet.data.assign(dataStart, dataEnd);
		}
        currentPos += dataLength;

		// Step4: 读取校验和
		packet.sSum = *reinterpret_cast<const WORD*>(&buffer[currentPos]);
		currentPos += sizeof(WORD);

		// Step5: 校验
        WORD calculatedSum = 0;
        if(!packet.data.empty()) {
            calculatedSum = std::accumulate(packet.data.begin(), packet.data.end(), static_cast<WORD>(0));
		}
        if(calculatedSum != packet.sSum) {
            bytesconsumed += sizeof(WORD);
            return std::nullopt; 
		}

		bytesconsumed = currentPos;
		return packet;
		} 

    // 发送当前 Cpacket 为字节流
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
            exit(0);
        }
    }
    ~WSAInitializer() {
        WSACleanup();
    }
};

class CServerSocket {
public:
	explicit CServerSocket(unsigned short port) {
		m_servSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_servSocket == INVALID_SOCKET) {
			MessageBoxW(NULL, L"Socket creation failed", L"Error", MB_OK);
			exit(0);
		}

		sockaddr_in serveraddr;
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_addr.s_addr = INADDR_ANY;
		serveraddr.sin_port = htons(port);

		if (::bind(m_servSocket, reinterpret_cast<sockaddr*>(&serveraddr), sizeof(serveraddr)) == SOCKET_ERROR ) {
			MessageBoxW(NULL, L"Bind failed", L"Error", MB_OK);
			closesocket(m_servSocket);
			exit(0);
		}

		if (listen(m_servSocket, 1) == SOCKET_ERROR) {
			MessageBoxW(NULL, L"Listen failed", L"Error", MB_OK);
			closesocket(m_servSocket);
			exit(0);
		}
	}

	~CServerSocket() {
		if (m_clientSocket != INVALID_SOCKET) {
			closesocket(m_clientSocket);
		}
		if (m_servSocket != INVALID_SOCKET) {
			closesocket(m_servSocket);
		}
	}
	
	virtual void SendPacket(const Cpacket& packet) {
		auto buffer = packet.SerializePacket();
		send(m_clientSocket, reinterpret_cast<const char*>(buffer.data()), buffer.size(), 0);
	}
	
	void SendErrorPacket(const std::string& errorMessage) {
		std::vector<BYTE> errPayload(errorMessage.begin(), errorMessage.end());
		Cpacket errPacket(CMD_ERROR, errPayload);
		SendPacket(errPacket);
	}
	
	std::optional<Cpacket> RecvPacket() {
		while (true) {
			size_t bytesConsumed = 0;
			auto packetOpt = Cpacket::DeserializePacket(m_recvBuffer, bytesConsumed);
			if (packetOpt) {
				m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + bytesConsumed);
				return packetOpt;
			}
			if (bytesConsumed > 0) {
				m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + bytesConsumed);
				continue;
			}
			// 接收更多数据
			std::vector<BYTE> tempBuffer(BUFFER_SIZE);
			int bytesReceived = recv(m_clientSocket, reinterpret_cast<char*>(tempBuffer.data()), BUFFER_SIZE, 0);
			if (bytesReceived <= 0) {
				return std::nullopt;
			}
			m_recvBuffer.insert(m_recvBuffer.end(), tempBuffer.begin(), tempBuffer.begin() + bytesReceived);
		}
	}
	
	CServerSocket(const CServerSocket&) = delete;
	CServerSocket& operator=(const CServerSocket&) = delete;

	void Run(const std::function<void(const Cpacket&)>& packetHandler) {
		while (true) {
			std::cout << "Waiting for client connection..." << std::endl;

			m_clientSocket = accept(m_servSocket, nullptr, nullptr);
			if (m_clientSocket == INVALID_SOCKET) {
				MessageBoxW(NULL, L"Accept failed", L"Error", MB_OK);
				continue;
			}
			std::cout << "Client connected." << std::endl;
			HandleClient(packetHandler);

			closesocket(m_clientSocket);
			m_clientSocket = INVALID_SOCKET;
			m_recvBuffer.clear();
			std::cout << "Client disconnected." << std::endl;
		}
	}

private:
	SOCKET m_servSocket = INVALID_SOCKET;
	SOCKET m_clientSocket = INVALID_SOCKET;
	std::vector<BYTE> m_recvBuffer;
	static constexpr size_t BUFFER_SIZE = 4096;
	WSAInitializer m_wsaInit; 

	
	void HandleClient(const std::function<void(const Cpacket&)>& packetHAndler) {
		while (true) {
			auto packetOpt = RecvPacket();
			if (!packetOpt) {
				break; // 连接关闭或错误
			}
			packetHAndler(*packetOpt);
		}
	}
};
