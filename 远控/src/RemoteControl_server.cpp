// 远控.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteControl_server.h"
#include "ServerSocket.h"
#include <sstream>

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

void TestPacket() {
    Cpacket packet;
    packet.sHead = 0xFEFF;
    packet.sCmd = 0x1234; // 示例命令字
    packet.data = {0x01, 0x02, 0x03, 0x04}; // 示例数据
    packet.nLength = sizeof(packet.sCmd) + packet.data.size() + sizeof(packet.sSum); // 计算 nLength
    auto buffer = packet.SendPacket();
    std::stringstream ss;
    ss << "Packet in hex: ";
    for (auto b : buffer) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
    }
    OutputDebugStringA(ss.str().c_str());
}

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
                CServerSocket serverSocket(12345); // 监听端口12345

                auto myPacketHandler = [](const Cpacket& packet) {
                  std::wcout << L"Received packet - Cmd: " << packet.sCmd 
                             << L", Length: " << packet.nLength 
                             << L", Data Size: " << packet.data.size() 
                             << L", Checksum: " << packet.sSum << std::endl;
                  // TODO: 根据命令字处理不同请求
                };
                
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
        wprintf(L"错误: GetModuleHandle 失败\n");
        nRetCode = 1;
    }

    return nRetCode;
}
