// 远控.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteControl_server.h"
#include "ServerSocket.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#pragma comment (linker , "/subsystem:windows /ENTRY:mainCRTStartup")
#pragma comment (linker , "/subsystem:windows /ENTRY:WinMainCRTStartup")
#pragma comment (linker , "/subsystem:console /ENTRY:mainCRTStartup")
#pragma comment (linker , "/subsystem:console /ENTRY:WinMainCRTStartup")

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
            try {
                auto myPacketHandler = [](const Cpacket& packet) {
                    
                  std::wcout << L"Received packet - Cmd: " << packet.sCmd 
                             << L", Length: " << packet.nLength 
                             << L", Data Size: " << packet.data.size() 
					         << L", Checksum: " << packet.sSum << std::endl;
                    // 这里可以添加更多处理逻辑

					};
                CServerSocket serverSocket(12345); // 监听端口12345
                serverSocket.Run(myPacketHandler);
            }
            catch (const std::exception& ex) {
				std::cerr << "Exception: " << ex.what() << std::endl;
				return  1;
          }
			return 0;
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
