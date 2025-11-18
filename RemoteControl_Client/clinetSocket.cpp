#include "pch.h"
#include "clientSocket.h"
#include <string>

inline std::string GetErrInfo(int nError) {
    std::string strError;
    LPVOID lpMsgBuf = nullptr;

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        nError,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0,
        NULL);

    strError = (char*)lpMsgBuf;
    LocalFree(lpMsgBuf);
    return strError;
}

// 设置用于屏幕显示的窗口句柄
void CClientSocket::SetScreenViewWnd(HWND hWnd)
{
    m_hScreenViewWnd = hWnd;
}