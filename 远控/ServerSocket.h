#pragma once
#include "pch.h"
#include "framework.h"

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

    int DealCommand() {
        if (m_client == -1) return false;
        char Buffer[1024] = " ";

        while (true) {
            int len = recv(m_client, Buffer, sizeof(Buffer), 0);
            if (len <= 0) return -1;
        }
    }

    bool Send(const void* pData, size_t size) {
        if (m_client == -1) return false;
        return send(m_client, (const char*)pData, size, 0) > 0;
    }
};