#include "pch.h"
#include "pch.h"
#include "FileCommandHandler.h"
#include "..\ServerSocket.h"
#include "..\RemoteControl_Server\include\core\Enities.h"
#include <string>
#include <cstdio>
#include <cstring>
#include <vector>
#include <io.h>
#include <direct.h>
#include <windows.h>

using namespace std;

namespace handlers {

static void Dump(const unsigned char *pData, size_t nSize) {
  std::string strOut;
  for (size_t i = 0; i < nSize; i++) {
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

// local FILEINFO structure used for directory listing packets
typedef struct file_info {
  file_info() {
    isValid = 0;
    isDir = 0;
    hasNext = true;
    memset(szFileName, 0, sizeof(szFileName));
  }

  bool isValid;
  bool isDir;
  char szFileName[256];
  bool hasNext;

} FILEINFO, *PFILEINFO;

int HandleMakeDriverInfo(CServerSocket &socket) {
  std::string result;
  DWORD mask = GetLogicalDrives();
  for (int i = 0; i < 26; ++i) {
    if (mask & (1u << i)) {
      if (!result.empty())
        result += ",";
      result += char('A' + i);
    }
  }

  Cpacket pack(CMD::CMD_LIST_DRIVES, reinterpret_cast<const BYTE *>(result.data()), result.size());
  Dump(reinterpret_cast<const unsigned char *>(pack.Data()), pack.Size());
  if (!socket.Send(pack)) {
    OutputDebugStringA("发送驱动器信息失败\n");
    return -1;
  }
  return 0;
}

int HandleMakeDirectoryInfo(CServerSocket &socket) {
  std::string path;
  if (!socket.GetFilePath(path)) {
    OutputDebugStringA("HandleMakeDirectoryInfo: get path failed\n");
    return -1;
  }

  std::string pattern = path;
  if (!pattern.empty() && pattern.back() != '\\')
    pattern += '\\';
  pattern += "*";

  _finddata_t fdata;
  intptr_t hfind = _findfirst(pattern.c_str(), &fdata);
  if (hfind == -1) {
    OutputDebugStringA("HandleMakeDirectoryInfo: no files found\n");
    return -3;
  }

  do {
    if (strcmp(fdata.name, ".") == 0 || strcmp(fdata.name, "..") == 0)
      continue;
    FILEINFO finfo;
    finfo.isValid = true;
    finfo.isDir = (fdata.attrib & _A_SUBDIR) != 0;
    size_t len = strlen(fdata.name);
    size_t copy_len = std::min<size_t>(len, sizeof(finfo.szFileName) - 1);
    memcpy(finfo.szFileName, fdata.name, copy_len);
    finfo.szFileName[copy_len] = '\0';
    Cpacket pack(CMD::CMD_LIST_DIR, (BYTE *)&finfo, sizeof(finfo));
    socket.Send(pack);
  } while (_findnext(hfind, &fdata) == 0);

  _findclose(hfind);

  FILEINFO finfo;
  // 终止包：标记为无效并且没有下一个条目
  finfo.isValid = true;
  finfo.hasNext = false;
  Cpacket pack(CMD::CMD_LIST_DIR, (BYTE *)&finfo, sizeof(finfo));
  socket.Send(pack);

  return 0;
}

int HandleRunFile(CServerSocket &socket) {
  std::string path;
  if (!socket.GetFilePath(path)) {
    OutputDebugStringA("HandleRunFile: get path failed\n");
    return -1;
  }
  ShellExecuteA(NULL, NULL, path.c_str(), NULL, NULL, SW_SHOWNORMAL);
  Cpacket pack(CMD::CMD_RUN_FILE, NULL, 0);
  socket.Send(pack);
  return 0;
}

int HandleDeleteFile(CServerSocket &socket) {
  std::string path;
  if (!socket.GetFilePath(path)) {
    OutputDebugStringA("HandleDeleteFile: get path failed\n");
    Cpacket err(CMD::CMD_ERROR, NULL, 0);
    socket.Send(err);
    return -1;
  }

  int ret = remove(path.c_str());
  if (ret == 0) {
    Cpacket ok(CMD::CMD_DELETE_FILE, NULL, 0);
    socket.Send(ok);
    return 0;
  } else {
    char buf[128];
    snprintf(buf, sizeof(buf), "HandleDeleteFile failed, errno=%d", errno);
    OutputDebugStringA(buf);
    Cpacket err(CMD::CMD_ERROR, (BYTE *)buf, (int)strlen(buf));
    socket.Send(err);
    return -2;
  }
}

int HandleDownloadFile(CServerSocket &socket) {
  std::string path;
  if (!socket.GetFilePath(path)) {
    OutputDebugStringA("HandleDownloadFile: get path failed\n");
    Cpacket errpack(CMD::CMD_DOWNLOAD_FILE, NULL, 0);
    socket.Send(errpack);
    return -1;
  }

  FILE *fp = nullptr;
  if (fopen_s(&fp, path.c_str(), "rb") != 0 || fp == NULL) {
    OutputDebugStringA("HandleDownloadFile: open failed\n");
    Cpacket errpack(CMD::CMD_DOWNLOAD_FILE, NULL, 0);
    socket.Send(errpack);
    return -1;
  }

  _fseeki64(fp, 0, SEEK_END);
  long long fileSize = _ftelli64(fp);
  _fseeki64(fp, 0, SEEK_SET);

  unsigned char sizeBuf[sizeof(fileSize)];
  memcpy(sizeBuf, &fileSize, sizeof(fileSize));
  Cpacket head(CMD::CMD_DOWNLOAD_FILE, (BYTE *)sizeBuf, sizeof(fileSize));
  socket.Send(head);

  const size_t CHUNK = 4096;
  std::vector<char> buffer(CHUNK);
  size_t nRead = 0;
  while ((nRead = fread(buffer.data(), 1, CHUNK, fp)) > 0) {
    Cpacket pack(CMD::CMD_DOWNLOAD_FILE, (BYTE *)buffer.data(), nRead);
    if (!socket.Send(pack)) {
      OutputDebugStringA("HandleDownloadFile: send chunk failed\n");
      break;
    }
  }
  fclose(fp);

  Cpacket eofpack(CMD::CMD_EOF, NULL, 0);
  socket.Send(eofpack);

  return 0;
}

} // namespace handlers
