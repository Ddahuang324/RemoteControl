// 远控.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "RemoteControl_server.h"
#include "../RemoteControl_Server/include/core/Enities.h"
#include "ServerSocket.h"
#include "framework.h"
#include "direct.h"
#include "io.h"
#include <memory>
#include <atlimage.h>
#include <atomic>
#include <list>
#include <thread>
#include <vector>

typedef struct file_info {
  file_info() {
    isValid = 0;
    isDir = 0;
    hasNext = true;
    memset(szFileName, 0, sizeof(szFileName));
  }

  bool isValid;         // 是否有效
  bool isDir;           // 是目录还是文件
  char szFileName[256]; // 文件名
  bool hasNext;         // 是否有下一个

} FILEINFO, *PFILEINFO;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
// Remove conflicting linker pragmas that forced different entry points.
// The project settings determine the correct subsystem/entry for this build.

// 唯一的应用程序对象

CWinApp theApp;

using namespace std;
#include "ServerController.h"
int main() {
  int nRetCode = 0;

  HMODULE hModule = ::GetModuleHandle(nullptr);

  if (hModule != nullptr) {
    // 初始化 MFC 并在失败时显示错误
    if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0)) {
      // TODO: 在此处为应用程序的行为编写代码。
      wprintf(L"错误: MFC 初始化失败\n");
      nRetCode = 1;
    } else {
      AllocConsole(); // 分配控制台
      FILE *pFile;
      freopen_s(&pFile, "CONOUT$", "w", stdout); // 重定向 stdout 到控制台
      wprintf(L"控制台初始化成功。\n");

      // 使用面向对象的 ServerController 管理连接和命令处理
      CServerSocket &serverSocket = CServerSocket::GetInstance();
      ServerController controller(serverSocket);
     
      
      int runRet = controller.Run();
      if (runRet != 0) {
        wprintf(L"服务器运行返回错误: %d\n", runRet);
      }
      nRetCode = runRet;
    }
  } else {
    // TODO: 更改错误代码以符合需要
    wprintf(L"错误: GetModuleHandle 失败\n");
    nRetCode = 1;
  }

  return nRetCode;
}
