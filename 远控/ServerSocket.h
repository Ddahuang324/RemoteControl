#pragma once
#include "pch.h"
#include "framework.h"

class Cpacket {
public:
	Cpacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
    Cpacket(WORD nCmd, const BYTE* pData, size_t nSize) {
		sHead = 0xFEFF;
		nLength = nSize + 2 + 2;//数据长度=数据+命令字+校验和
		sCmd = nCmd;

        if (nSize > 0) {
            strData.resize(nSize);
            memcpy((void*)strData.c_str(), pData, nSize);
        }
        else {
			strData.clear();
        }
		sSum = 0;
        for (size_t j = 0; j < strData.size(); j++) {//计算校验和
            sSum += BYTE(strData[j]) & 0xFF;
        }
    }
    Cpacket(const Cpacket& pkt) {
		sHead = pkt.sHead;
		nLength = pkt.nLength;
		sCmd = pkt.sCmd;
        strData = pkt.strData;
		sSum = pkt.sSum;
    }
    Cpacket(const BYTE* pData, size_t& nSize) {
		size_t  i = 0;

        for(; i < nSize; i++) {
          if(*(WORD*)((char*)pData + i) == 0xFEFF) {
			  sHead = *(WORD*)((char*)pData + i);
			  i += 2;//跳过已经读取的包头，防止读取错误
              break;
		  }
		}//解析包头

        if (i + 4 + 2 + 2 > nSize) {//边界检查，用于确保从当前偏移 i 开始，数据缓冲区 pData 中至少有足够字节来解析后续的固定字段
			nSize = 0;//用掉了0个字节，这个函数的输入是包的总长度，输出是用掉的字节数
            return;
        }//如果没有找到包头，解析失败，直接返回
        
		nLength = *(DWORD*)((char*)pData + i); i += 4;//数据长度
		sCmd = *(WORD*)((char*)pData + i); i += 2;//命令字
		if (i + (nLength -2 -2) + 2  > nSize) {//边界检查，用于确保从当前偏移 i 开始，数据缓冲区 pData 中至少有足够字节来解析后续的数据字段和校验和字段
			nSize = 0;//用掉了0个字节，这个函数的输入是包的总长度，输出是用掉的字节数
            return;
        }

        if (nLength > 4) {//数据长度至少要大于4，才能包含命令字和校验和
            strData.resize(nLength - 2 - 2);
            memcpy((void*)strData.c_str(), (char*)pData + i, nLength - 2 - 2);
			i += nLength - 2 - 2;//数据
        }
        sSum = *(WORD*)((char*)pData + i); i += 2;//校验和
		WORD sum = 0;
		for (size_t j = 0; j < strData.size(); j++) {//计算校验和
			sum += BYTE(strData[j]) & 0xFF ;
		}
		if (sum == sSum) {//校验和正确
            nSize = i;
            return;
        }
		nSize = 0;//用掉了0个字节，解析失败
    }
	~Cpacket() {}
    Cpacket& operator=(const Cpacket& pkt) {
        if (this != &pkt) {
            sHead = pkt.sHead;
            nLength = pkt.nLength;
            sCmd = pkt.sCmd;
            strData = pkt.strData;
            sSum = pkt.sSum;
        }
		return *this;
    }

    int Size() {//包数据大小
		return nLength + 2 + 4;
    }
	const char* Data() {//包数据指针
		strOut.resize(nLength + 6);
		BYTE* pData = (BYTE*)strOut.c_str();
		*(WORD*)pData = sHead; pData += 2;
		*(DWORD*)pData = nLength; pData += 4;
		*(WORD*)pData = sCmd; pData += 2;
        memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
		*(WORD*)pData = sSum;
        
		return strOut.c_str();
    }
public:
	WORD sHead;//包头,固定位FEFF
	DWORD nLength;//数据长度
	WORD sCmd;//命令字
	std::string strData;//数据
	WORD sSum;//校验和

	std::string strOut;//包的数据，用于调试
};

class CServerSocket
{
private:
    CServerSocket(const CServerSocket& ss) {
        m_client = ss.m_client;
        m_serv = ss.m_serv;
    }
    CServerSocket& operator=(const CServerSocket& ss) {}
    CServerSocket() {
		m_serv = INVALID_SOCKET;
		m_client = INVALID_SOCKET;

        if (InitSocketEnv() == false) {
            // Initialization failed
            MessageBoxW(NULL, L"Initialization failed", L"Error", MB_OK);
            exit(0);
        }

        m_serv = socket(PF_INET, SOCK_STREAM, 0);
        if (m_serv == INVALID_SOCKET) {
            // Socket creation failed, handle error
            MessageBoxW(NULL, L"Socket creation failed", L"Error", MB_OK);
            WSACleanup();
            exit(0);
        }
        if (InitSocketEnv() == false) {
            // Initialization failed
            MessageBoxW(NULL, L"Initialization failed", L"Error", MB_OK);
            exit(0);
        }
    }
    ~CServerSocket() {
        closesocket(m_serv);
        WSACleanup();
    };

    bool InitSocketEnv() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
            return false;
        }
        return true;
    };

    SOCKET m_serv;
    SOCKET m_client;
	Cpacket m_packet;

public:

    static void CleanUp() {
        WSACleanup();
    }
    static CServerSocket& GetInstance() {
        static CServerSocket instance;
        return instance;
    }

    bool initSocket() {

        //校验
        if (m_serv == -1) {
            return false;
        }

        sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        serv_addr.sin_port = htons(12345);

        if (bind(m_serv, (SOCKADDR*)&serv_addr, sizeof(SOCKADDR)) == -1) {
            return false;
        }
        if (listen(m_serv, 1) == -1) {
            return false;
        }

        return true;

    }
    bool AcceptClient() {
        sockaddr_in client_addr;
        char buffer[1024]; // buffer 未使用导致 C4101
        int client_addr_size = sizeof(SOCKADDR);
        m_client = accept(m_serv, (SOCKADDR*)&client_addr, &client_addr_size);
        if (m_client == -1)  return false;

        // 使用 buffer，避免未引用警告
        int recvLen = recv(m_client, buffer, sizeof(buffer), 0);
        if (recvLen > 0) {
            // 可以根据实际需求处理接收到的数据
        }

        return true;
    }

#define BUFFER_SIZE 4096

    int DealCommand() {
        if (m_client == -1) return false;
        //char Buffer[1024] = " ";
		auto Buffer = std::make_unique < char[]> (BUFFER_SIZE);
		memset(Buffer.get(), 0, BUFFER_SIZE);
        size_t index = 0;
        while (true) {
            int len = recv(m_client, Buffer.get() + index, BUFFER_SIZE - index, 0);
            if (len <= 0) return -1;
			index += len;
			len = index;
			m_packet = Cpacket (reinterpret_cast<const BYTE*>(Buffer.get()), (size_t&)len);
             
            if (len > 0) {
				memmove(Buffer.get(), Buffer.get() + len, BUFFER_SIZE - len);
				index -= len;
                return m_packet.sCmd;
            }
        } 
		return -1;
    }

    bool Send(const void* pData, size_t size) {
        if (m_client == -1) return false;
        return send(m_client, (const char*)pData, size, 0) > 0;
    }
    bool Send(Cpacket& pkt) {
        if (m_client == -1) return false;
		return send(m_client,  pkt.Data(), pkt.Size(), 0) > 0;
	}

    bool GetFilePath(std::string& strPath) {
        if ((m_packet.sCmd == 2)|| (m_packet.sCmd == 3)|| (m_packet.sCmd == 4)){ 
            strPath = m_packet.strData;
			return true;
        }
		return false;
	}
};