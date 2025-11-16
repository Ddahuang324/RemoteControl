// RemoteControl_Server.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteControl_server.h"
#include "ServerSocket.h"
#include "Enities.h"
#include "fileSystem.h"
#include "InputeSimulator.h"
#include "screenCapture.h"
#include "LockMachine.h"
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
            // GDI+ startup
            Gdiplus::GdiplusStartupInput gdiplusStartupInput;
            ULONG_PTR gdiplusToken;
            Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

            try {
                CServerSocket serverSocket(12345); // 监听端口12345

                auto myPacketHandler = [&serverSocket](const Cpacket& packet) {
                  std::wcout << L"Received packet - Cmd: " << packet.sCmd 
                             << L", Length: " << packet.nLength 
                             << L", Data Size: " << packet.data.size() 
                             << L", Checksum: " << packet.sSum << std::endl;
                  
                  // 根据命令字处理不同请求
                  switch (packet.sCmd) {
                      case CMD::CMD_DRIVER_INFO: {
                          std::string driveInfo = GetDriverInfo();
                          std::vector<BYTE> data(driveInfo.begin(), driveInfo.end());
                          Cpacket response(CMD::CMD_DRIVER_INFO, data);
                          serverSocket.SendPacket(response);
                          break;
                      }
                      case CMD::CMD_DIRECTORY_INFO: {
                          if (!packet.data.empty()) {
                              std::string path(packet.data.begin(), packet.data.end());
                              DirectoryInfor(path, serverSocket);
                          }
                          break;
                      }
                      case CMD::CMD_RUN_FILE: {
                          if (!packet.data.empty()) {
                              std::string path(packet.data.begin(), packet.data.end());
                              RunFile(path, serverSocket);
                          }
                          break;
                      }
                       case CMD::CMD_DOWNLOAD_FILE: {
                          if (!packet.data.empty()) {
                              std::string path(packet.data.begin(), packet.data.end());
                              DownloadFile(path, serverSocket);
                          }
                          break;
                      }
                      case CMD::CMD_DELETE_FILE: {
                          if (!packet.data.empty()) {
                              std::string path(packet.data.begin(), packet.data.end());
                              DeleteFile(path, serverSocket);
                          }
                          break;
                      }
                      case CMD::CMD_MOUSE_EVENT: {
                          HandleMouseEvent(serverSocket, packet);
                          break;
                      }
                      case CMD::CMD_SCREEN_CAPTURE: {
                          CaptureScreen(serverSocket, packet);
                          break;
                      }
                        case CMD::CMD_LOCK_MACHINE: {
                            
                            LockMachine(serverSocket, packet);
                            break;
                      }
                        case CMD::CMD_UNLOCK_MACHINE: {
                            UnlockMachine(serverSocket, packet);
                            break;
                      }
                        case CMD::CMD_TEST_CONNECT: {
                          std::wcout << L"Test Connect (2002) received. Sending ACK." << std::endl;
                          // 简单地回送一个相同命令字的包作为确认
                          Cpacket response(CMD::CMD_TEST_CONNECT, {});
                          serverSocket.SendPacket(response);
                          break;
                      }
                      default: {
                          std::string errMsg = "Unknown command";
                          serverSocket.SendErrorPacket(errMsg);
                          break;
                      }
                  }
                };
                
                serverSocket.Run(myPacketHandler);
            }
            catch (const std::exception& ex) {
                std::cerr << "Exception: " << ex.what() << std::endl;
                Gdiplus::GdiplusShutdown(gdiplusToken);
                return  1;
            }
            Gdiplus::GdiplusShutdown(gdiplusToken);
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
