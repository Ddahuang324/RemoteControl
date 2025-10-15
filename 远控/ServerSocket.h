#pragma once
#include "pch.h"
#include "framework.h"
#include <numeric>
#include <optional>
#include <string>
#include <vector>
#include <functional>
#include <winsock2.h>

using BYTE = unsigned char;
using WORD = unsigned short;
using DWORD = unsigned long;

class Cpacket {
public:
	Cpacket() = default;

    //数据格式
	WORD sHead;//包头,固定位FEFF
	DWORD nLength;//数据长度(除头包以外，剩余三部分的长度)
	WORD sCmd;//命令字
	std::vector<BYTE> data;//数据
	WORD sSum;//校验和

	//工厂函数，用于从缓冲区安全的创建 Cpacket 对象
    //buffer：输入数据的缓冲区
	//bytesconsumed：输出参数，表示从缓冲区中消耗的字节数
    static std::optional <Cpacket> FromBuffer(const std::vector<BYTE>& buffer, size_t& bytesconsumed) {
		bytesconsumed = 0;
		size_t bufferSize = buffer.size();

        //Step1:寻找包头
        size_t headpos = 0;
        bool headFound = false;
        for (; headpos + sizeof(WORD) <= bufferSize; ++headpos) {
            //使用reinterpret_cast保证从 字节数组中读取一个word
            //边界已由for保证
            if (*reinterpret_cast<const WORD*>(&buffer[headpos]) == 0xFEFF) {
                headFound = true;
                break;
            }
        }

        if (!headFound) {
		    bytesconsumed = (bufferSize > 0) ? bufferSize - 1 : 0; // 保留最后一个字节以防包头跨缓冲区，导致流式包解析失败
			return std::nullopt; // 未找到包头，等待更多数据
        }
		
        bytesconsumed = headpos; // 消耗掉无效数据
		size_t remainingSize = bufferSize - headpos; // 计算剩余数据大小

		const size_t minPacketSize = sizeof(sHead) + sizeof(nLength) + sizeof(sCmd) + sizeof(sSum); //固定字段是否够长度
        
        if (remainingSize < minPacketSize) {// 不足以包含最小包头，等待更多数据
			return std::nullopt;
        }

		Cpacket packet;
		size_t currentPos = headpos;

		//Step2:读取固定字段
		packet.sHead = *reinterpret_cast<const WORD*>(&buffer[currentPos]);
		currentPos += sizeof(WORD);
		packet.nLength = *reinterpret_cast<const DWORD*>(&buffer[currentPos]);
		currentPos += sizeof(DWORD);
		packet.sCmd = *reinterpret_cast<const WORD*>(&buffer[currentPos]);
		currentPos += sizeof(WORD);


        if(packet.nLength < (sizeof(sCmd) + sizeof(sSum)) ) {//长度字段不合理
            bytesconsumed += sizeof(WORD); // 只消耗包头，继续寻找下一个包头
            return std::nullopt; // 包长度不合理，等待更多数据
		}

		DWORD dataLength = packet.nLength - sizeof(sCmd) - sizeof(sSum); //计算数据字段长度

		//Step3:检查数据字段是否超出缓冲区
        if ((currentPos - headpos) + dataLength + sizeof(sSum) > remainingSize) {
			return std::nullopt; // 数据字段不完整，等待更多数据
        }
        
		if (dataLength > 0) {//拷贝数据字段
			const auto dataStart = buffer.begin() + currentPos;
			const auto dataEnd = dataStart + dataLength;
			packet.data.assign(dataStart, dataEnd);
		}
        //解析完毕，移动位置
		currentPos += dataLength;

		//Step4:读取校验和
		packet.sSum = *reinterpret_cast<const WORD*>(&buffer[currentPos]);
		currentPos += sizeof(WORD);

		//Step5:验证校验和
        WORD calculatedSum = 0;

        if(!packet.data.empty()) {
            calculatedSum = std::accumulate(packet.data.begin(), packet.data.end(), static_cast<WORD>(0));
		}

        if(calculatedSum != packet.sSum) {
            bytesconsumed += sizeof(WORD); // 只消耗包头，继续寻找下一个包头
            return std::nullopt; // 校验和错误，等待更多数据
		}

		bytesconsumed = currentPos; // 成功解析一个完整包，更新消耗字节数
		return packet;
		} 
};

class WSAInitializer {
public:
    WSAInitializer() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            // Handle error
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
		//winsocket 已经在成员变量中初始化

		// 创建服务器监听套接字
		m_servSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_servSocket == INVALID_SOCKET) {
			MessageBoxW(NULL, L"Socket creation failed", L"Error", MB_OK);
			exit(0);
		}

		sockaddr_in serveraddr;
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_addr.s_addr = INADDR_ANY;
		serveraddr.sin_port = htons(port);

		// 绑定套接字
		if (bind(m_servSocket, reinterpret_cast<sockaddr*>(&serveraddr), sizeof(serveraddr)) == -1 ) {
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
	
	CServerSocket(const CServerSocket&) = delete;
	CServerSocket& operator=(const CServerSocket&) = delete;

	void Run(const std::function<void(const Cpacket&)>& packetHandler) {
		while (true) {
			std::cout << "Waiting for client connection..." << std::endl;
			m_clientSocket = accept(m_servSocket, nullptr, nullptr);
			if (m_clientSocket == INVALID_SOCKET) {
				MessageBoxW(NULL, L"Accept failed", L"Error", MB_OK);
				continue; // 继续等待下一个连接
			}
			std::cout << "Client connected." << std::endl;
			HandleClient(packetHandler);

			// 关闭客户端连接
			closesocket(m_clientSocket);
			m_clientSocket = INVALID_SOCKET;
			m_recvBuffer.clear(); // 清空接收缓冲区，为下一个客户端连接做准备
			std::cout << "Client disconnected." << std::endl;
		}
	}

private:
	SOCKET m_servSocket = INVALID_SOCKET;
	SOCKET m_clientSocket = INVALID_SOCKET;
	std::vector<BYTE> m_recvBuffer;
	static constexpr size_t BUFFER_SIZE = 4096;
	WSAInitializer m_wsaInit; // 确保在任何套接字操作前初始化 Winsock


	//方法
	void HandleClient(const std::function<void(Cpacket&)>& packetHAndler) {
		const size_t BUFFER_SIZE = 4096;
		std::vector<BYTE> tempBuffer(BUFFER_SIZE);

		while (true) {
			int bytesReceived = recv(m_clientSocket, reinterpret_cast<char*>(tempBuffer.data()), BUFFER_SIZE, 0);
			
			if (bytesReceived <= 0) {
				break; // 接收错误，退出循环
			}

			m_recvBuffer.insert(m_recvBuffer.end(), tempBuffer.begin(), tempBuffer.begin() + bytesReceived);

			while(!m_recvBuffer.empty()) {
				size_t bytesConsumed = 0;
				
				auto packetOpt = Cpacket::FromBuffer(m_recvBuffer, bytesConsumed);
				
				if (packetOpt) {
					packetHAndler(*packetOpt); // 处理完整包
					m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + bytesConsumed);
				}
				if (bytesConsumed > 0) {
					m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + bytesConsumed);
				} else {
					break; // 没有消耗数据，等待更多数据
				}
			}
		}
	}
};
