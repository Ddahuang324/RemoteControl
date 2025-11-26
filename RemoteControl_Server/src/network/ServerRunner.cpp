#include "pch.h"
#include "ServerRunner.h"
#include "ServerSocket.h"
#include "CommandDispatcher.h"
#include <gdiplus.h>
#include <iostream>

ServerRunner::ServerRunner(unsigned short port) : port_(port) {}

int ServerRunner::Run()
{
    // GDI+ 启动
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok) {
        std::cerr << "Gdiplus startup failed\n";
        return 1;
    }

    try {
        CServerSocket serverSocket(port_);
        CommandDispatcher dispatcher;

        serverSocket.Run([&dispatcher, &serverSocket](const Cpacket& packet) {
            dispatcher.Dispatch(packet, serverSocket);
        });
    }
    catch (const std::exception& ex) {
        std::cerr << "ServerRunner exception: " << ex.what() << std::endl;
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 1;
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}
