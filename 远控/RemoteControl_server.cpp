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
			CServerSocket& server = CServerSocket::GetInstance();   
            int count = 0;
            if (server.initSocket()) {

                while (true){
                    if (server.AcceptClient()) {
                        server.DealCommand();
                    }
                    else {
                        if (count >= 3) {
                            MessageBox(NULL, L"AcceptClient 失败", L"错误", MB_OK);
                        }
                        exit(0);
                    }
                }
          
			}
            else {
                MessageBox(NULL, L"initSocket 失败,初始化异常", L"错误", MB_OK);
                exit(0);
            }
               
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
