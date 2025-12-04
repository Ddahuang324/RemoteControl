// Clean UTF-8 version of RemoteControl_server.cpp
#include "pch.h"
#include "RemoteControl_server.h"
#include "ServerRunner.h"
#include "framework.h"
#include <iostream>
#include <exception>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Single application object
CWinApp theApp;

int main()
{
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // Initialize MFC and handle failure
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            wprintf(L"Error: MFC initialization failed\n");
            nRetCode = 1;
        }
        else
        {
            try
            {
                ServerRunner runner(9527);
                return runner.Run();
            }
            catch (const std::exception& ex)
            {
                std::cerr << "Exception: " << ex.what() << std::endl;
                return 1;
            }
        }
    }
    else
    {
        wprintf(L"Error: GetModuleHandle failed\n");
        nRetCode = 1;
    }

    return nRetCode;
}