// CommonHelpers.cpp
#include "pch.h"
#include "CommonHelpers.h"
#include <string>
#include <cstdio>
#include <windows.h>

void Dump(const unsigned char *pData, unsigned __int64 nSize) {
  std::string strOut;
  for (unsigned __int64 i = 0; i < nSize; ++i) {
    char buf[8] = "";
    if (i > 0 && i % 16 == 0) {
      strOut += "\n";
    }
    snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
    strOut += buf;
  }
  strOut += "\n";
  OutputDebugStringA(strOut.c_str());
}
