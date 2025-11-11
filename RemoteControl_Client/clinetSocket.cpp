#include "clientSocket.h"
#include "pch.h"
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