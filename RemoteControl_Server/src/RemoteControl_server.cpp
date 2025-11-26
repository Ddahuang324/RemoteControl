// RemoteControl_Server.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteControl_server.h"
#include "ServerRunner.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
// Removed conflicting linker pragmas so the project uses the
// linker subsystem/entry point defined in the project settings
// and the standard `main()` entry in this translation unit.

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
                ServerRunner runner(12345);
                return runner.Run();
            }
            catch (const std::exception& ex) {
                std::cerr << "Exception: " << ex.what() << std::endl;
                return  1;
            }
        }
    }
    else
    {
        wprintf(L"错误: GetModuleHandle 失败\n");
        nRetCode = 1;
    }

    return nRetCode;
}
