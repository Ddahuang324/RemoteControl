#pragma once
#include "pch.h"
#include "framework.h"
#include <ws2tcpip.h>

class Cpacket {
public:
	Cpacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
    Cpacket(WORD nCmd, const BYTE* pData, size_t nSize) {
		sHead = 0xFEFF;
        nLength = nSize + 2 + 2;
        sCmd = nCmd; // <-- 初始化命令码，避免未初始化内存(调试模式下为0xCCCC)被发送

        if (nSize > 0) {
            strData.resize(nSize);
            memcpy((void*)strData.c_str(), pData, nSize);
        }
        else {
			strData.clear();
        }
		sSum = 0;
        for (size_t j = 0; j < strData.size(); j++) {//����У���
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
			  i += 2;//�����Ѿ���ȡ�İ�ͷ����ֹ��ȡ����
              break;
		  }
		}//������ͷ

        if (i + 4 + 2 + 2 > nSize) {//�߽��飬����ȷ���ӵ�ǰƫ�� i ��ʼ�����ݻ����� pData ���������㹻�ֽ������������Ĺ̶��ֶ�
			nSize = 0;//�õ���0���ֽڣ���������������ǰ����ܳ��ȣ�������õ����ֽ���
            return;
        }//���û���ҵ���ͷ������ʧ�ܣ�ֱ�ӷ���
        
		nLength = *(DWORD*)((char*)pData + i); i += 4;//���ݳ���
		sCmd = *(WORD*)((char*)pData + i); i += 2;//������
		if (i + (nLength -2 -2) + 2  > nSize) {//�߽��飬����ȷ���ӵ�ǰƫ�� i ��ʼ�����ݻ����� pData ���������㹻�ֽ������������������ֶκ�У����ֶ�
			nSize = 0;//�õ���0���ֽڣ���������������ǰ����ܳ��ȣ�������õ����ֽ���
            return;
        }

        if (nLength > 4) {//���ݳ�������Ҫ����4�����ܰ��������ֺ�У���
            strData.resize(nLength - 2 - 2);
            memcpy((void*)strData.c_str(), (char*)pData + i, nLength - 2 - 2);
			i += nLength - 2 - 2;//����
        }
        sSum = *(WORD*)((char*)pData + i); i += 2;//У���
		WORD sum = 0;
		for (size_t j = 0; j < strData.size(); j++) {//����У���
			sum += BYTE(strData[j]) & 0xFF ;
		}
		if (sum == sSum) {//У�����ȷ
            nSize = i;
            return;
        }
		nSize = 0;//�õ���0���ֽڣ�����ʧ��
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

    int Size() {//�����ݴ�С
		return nLength + 2 + 4;
    }

  
	const char* Data() {//������ָ��
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
	WORD sHead;
	DWORD nLength;
	WORD sCmd;
	std::string strData;
	WORD sSum;

	std::string strOut;
};

typedef struct MouseEvent {
    MouseEvent() :nAction(0), nButton(-1) {
        ptXY.x = 0;
        ptXY.y = 0;
    }
    WORD nAction;//点击 移动 双击
    WORD nButton;//鼠标按键 左/右/中 键
    POINT ptXY;//坐标
}MOUSEEVENT,*PMOUSEEVENT;

class CServerSocket
{
private:
    CServerSocket(const CServerSocket& ss) {
        m_client = ss.m_client;
        m_serv = ss.m_serv;
    }
    CServerSocket& operator=(const CServerSocket& ss) =default;
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
        // 打印创建的 socket 描述符，便于在终端查看
        printf("[ServerSocket] created server socket: %llu\n", (unsigned long long)m_serv);
        fflush(stdout);
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

        //У��
        if (m_serv == -1) {
            return false;
        }

        sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        serv_addr.sin_port = htons(12345);

        if (bind(m_serv, (SOCKADDR*)&serv_addr, sizeof(SOCKADDR)) == -1) {
            printf("[ServerSocket] bind failed on port %d\n", ntohs(serv_addr.sin_port));
            fflush(stdout);
            return false;
        }
        if (listen(m_serv, 1) == -1) {
            printf("[ServerSocket] listen failed\n"); fflush(stdout);
            return false;
        }

        printf("[ServerSocket] listening on port %d\n", ntohs(serv_addr.sin_port)); fflush(stdout);
        return true;

    }
    bool AcceptClient() {
        sockaddr_in client_addr;
        int client_addr_size = sizeof(SOCKADDR);
        m_client = accept(m_serv, (SOCKADDR*)&client_addr, &client_addr_size);
        if (m_client == INVALID_SOCKET) return false;

        // 使用 inet_ntop 替换 inet_ntoa
        char ip[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip)) == nullptr) {
            strcpy_s(ip, sizeof(ip), "unknown");
        }
        unsigned short port = ntohs(client_addr.sin_port);
        printf("[ServerSocket] accepted client: %s:%u (sock=%llu)\n", ip, (unsigned)port, (unsigned long long)m_client);
        fflush(stdout);
        return true;
    }

#define BUFFER_SIZE (4 * 1024 * 1024) // 4MB buffer to handle large PNG frames

    int DealCommand() {
        if (m_client == -1) return false;
        //char Buffer[1024] = " ";
		auto Buffer = std::make_unique < char[]> (BUFFER_SIZE);
        if(Buffer == nullptr){
            TRACE("Buffer is nullptr\n");
            return -2;
        }
		memset(Buffer.get(), 0, BUFFER_SIZE);
        size_t index = 0;
        while (true) {
            int len = recv(m_client, Buffer.get() + index, BUFFER_SIZE - index, 0);
            if (len <= 0) {
                printf("[ServerSocket] recv returned %d (client disconnected or error)\n", len);
                fflush(stdout);
                delete[] Buffer.release();
                return -1;
            }
			index += len;
			len = index;
			m_packet = Cpacket (reinterpret_cast<const BYTE*>(Buffer.get()), (size_t&)len);
             
            if (len > 0) {
                // 已成功解析出一个完整报文
                printf("[ServerSocket] parsed packet, cmd=%u\n", (unsigned)m_packet.sCmd);
                fflush(stdout);
                memmove(Buffer.get(), Buffer.get() + len, BUFFER_SIZE - len);
                index -= len;
                delete[] Buffer.release();
                return m_packet.sCmd;
            }
        } 
        delete[] Buffer.release();
		return -1;
    }

    bool Send(const void* pData, size_t size) {
        if (m_client == -1) return false;
        int ret = (int)send(m_client, (const char*)pData, (int)size, 0);
        printf("[ServerSocket] send -> client sock=%llu, bytes=%zu, ret=%d\n", (unsigned long long)m_client, size, ret);
        fflush(stdout);
        return ret > 0;
    }
    bool Send(Cpacket& pkt) {
		if (m_client == -1) return false;
		int ret = (int)send(m_client,  pkt.Data(), pkt.Size(), 0);
		printf("[ServerSocket] send(pkt) -> client sock=%llu, pkt_size=%d, ret=%d\n", (unsigned long long)m_client, pkt.Size(), ret);
		fflush(stdout);
		return ret > 0;
	}

    bool GetFilePath(std::string& strPath) {
        if ((m_packet.sCmd == 2)|| (m_packet.sCmd == 3)|| (m_packet.sCmd == 4)){ 
            strPath = m_packet.strData;
			return true;
        }
		return false;
	}
    
    bool GetMouseEvent(MOUSEEVENT& mouse) {
        if (m_packet.sCmd == 5) {
            memcpy(&mouse , m_packet.strData.c_str(),  sizeof(MOUSEEVENT));
            return true; 
        }
        return false;
    }

    operator bool() const { return m_client != INVALID_SOCKET; }

    Cpacket& GetPacket() {
        return m_packet;
    }
    void CloseClient() {
        printf("[ServerSocket] closing client socket: %llu\n", (unsigned long long)m_client); fflush(stdout);
        closesocket(m_client);
    }
};  
