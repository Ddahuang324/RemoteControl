// ServerController.cpp
#include "pch.h"
#include "ServerController.h"
#include <string>
#include <cstdio>
#include <atlimage.h>
#include <unordered_map>
#include <functional>
#include "../RemoteControl_Server/include/core/Enities.h"
#include "handlers/FileCommandHandler.h"
#include "handlers/MouseHandler.h"

using namespace std;

namespace {
void Dump(const BYTE *pData, size_t nSize) {
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
} // namespace

ServerController::ServerController(CServerSocket &serverSocket)
    : m_serverSocket(serverSocket) {
  // lazy create FileManager in Run after socket init/accept if needed
  // register internal handlers
  RegisterHandlers();
}

ServerController::~ServerController() {}

int ServerController::Run() {
  return Run(nullptr);
}

int ServerController::Run(ExecCallback execCb) {//纯粹网络连接和命令处理循环
  int count = 0;
  if (!m_serverSocket.initSocket()) {
    wprintf(L"错误: 初始化套接字失败\n");
    return -1;
  }

  // FileManager implementations moved into handlers; no helper object needed

  wprintf(L"服务器准备就绪，等待客户端连接...\n");

  while (true) {
    if (m_serverSocket.AcceptClient() == false) {
      if (count >= 3) {
        wprintf(L"错误: 客户端连接失败超过3次，程序退出。\n");
        exit(0);
      }
      wprintf(L"等待客户端连接失败，重试中...\n");
      count++;
      continue;
    }

    TRACE("客户端连接成功\n");
    while (true) {//处理命令
      int cmd = m_serverSocket.DealCommand();
      if (cmd > 0) {
        TRACE("收到命令: %d\r\n", cmd);
        // execute via provided callback if present, otherwise use internal map
        ExecuteCommand(cmd, execCb);
        continue;
      }
      wprintf(L"错误: 处理命令失败\n");
      break;
    }
    m_serverSocket.CloseClient();
  }

  return 0;
}

void ServerController::RegisterHandlers() {
  m_cmdHandlers.clear();
  // use member wrappers so FileManager pointer isn't captured at registration time
  m_cmdHandlers[1] = [this]() { return HandleMakeDriverInfo(); };
  m_cmdHandlers[2] = [this]() { return HandleMakeDirectoryInfo(); };
  m_cmdHandlers[3] = [this]() { return HandleRunFile(); };
  m_cmdHandlers[4] = [this]() { return HandleDownloadFile(); };
  m_cmdHandlers[11] = [this]() { return HandleDeleteFile(); };
  m_cmdHandlers[5] = [this]() { return HandleMouseEvent(); };
  m_cmdHandlers[6] = [this]() { return m_screenStreamer.SendOnce(); };
  m_cmdHandlers[10] = [this]() { m_screenStreamer.StartStream(100); return 0; };
  m_cmdHandlers[13] = [this]() { m_screenStreamer.StopStream(); return 0; };
  m_cmdHandlers[7] = [this]() { return m_lockManager.Lock(); };
  m_cmdHandlers[8] = [this]() { return m_lockManager.Unlock(); };
  m_cmdHandlers[2002] = [this]() { return HandleTestConnect(); };
}

int ServerController::ExecuteCommand(int nCmd, ExecCallback execCb) {
  // if external callback provided, invoke it first
  if (execCb) {
    return execCb(nCmd);
  }

  auto it = m_cmdHandlers.find(nCmd);
  if (it != m_cmdHandlers.end()) {
    try {
      return it->second();
    } catch (...) {
      return -1;
    }
  }
  return -1;
}

int ServerController::HandleMakeDriverInfo() {
  return handlers::HandleMakeDriverInfo(m_serverSocket);
}

int ServerController::HandleMakeDirectoryInfo() {
  return handlers::HandleMakeDirectoryInfo(m_serverSocket);
}

int ServerController::HandleRunFile() {
  return handlers::HandleRunFile(m_serverSocket);
}

int ServerController::HandleDeleteFile() {
  return handlers::HandleDeleteFile(m_serverSocket);
}

int ServerController::HandleDownloadFile() {
  return handlers::HandleDownloadFile(m_serverSocket);
}

int ServerController::HandleMouseEvent() {
  return handlers::HandleMouseEvent(m_serverSocket);
}

int ServerController::HandleTestConnect() {
  Cpacket pack(2002, NULL, 0);
  TRACE("测试连接命令收到，发送回执\n");
  bool ret = m_serverSocket.Send(pack);
  TRACE("测试连接命令回执发送结果： %d\n", ret);
  return 0;
}
