// 远控.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteControl_server.h"
#include "ServerSocket.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 唯一的应用程序对象

CWinApp theApp;

using namespace std;

int main()
{
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // 初始化 MFC 并在失败时显示错误
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: 在此处为应用程序的行为编写代码。
            wprintf(L"错误: MFC 初始化失败\n");
            nRetCode = 1;
        }
        else
        {
            // TODO: 在此处为应用程序的行为编写代码。
            //server;
            // TODO: 在此处为应用程序的行为编写代码。
            SOCKET serv = socket(PF_INET, SOCK_STREAM, 0);
            //校验
            sockaddr_in serv_addr, client_addr;
            memset(&serv_addr, 0, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
            serv_addr.sin_port = htons(12345);

            bind(serv, (SOCKADDR*)&serv_addr, sizeof(SOCKADDR));
            listen(serv, 1);
            char buffer[1024];
            int client_addr_size = sizeof(SOCKADDR);
            //accept(serv, (SOCKADDR*)&client_addr, &client_addr_size);
            //recv(serv, buffer, sizeof(buffer), 0);
            //send(serv, buffer, sizeof(buffer), 0);
            closesocket(serv);
        }
    }
    else
    {
        // TODO: 更改错误代码以符合需要
        wprintf(L"错误: GetModuleHandle 失败\n");
        nRetCode = 1;
    }

    return nRetCode;
}
