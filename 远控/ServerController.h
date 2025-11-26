// ServerController.h
#pragma once

#include "ServerSocket.h"
#include "ScreenStreamer.h"
#include "LockManager.h"
#include <memory>
#include <unordered_map>
#include <functional>

class ServerController {
public:
  explicit ServerController(CServerSocket &serverSocket);
  ~ServerController();

  // Run the main accept / command loop. Returns 0 on graceful exit.
  using ExecCallback = std::function<int(int)>;

  int Run();
  // Run with an external execution callback to decouple network and execution.
  int Run(ExecCallback execCb);

private:
  CServerSocket &m_serverSocket;
  ScreenStreamer m_screenStreamer;
  LockManager m_lockManager;

  // Execute a command. If an external callback is provided, it will be called
  // first; otherwise internal handler map will be used.
  int ExecuteCommand(int nCmd, ExecCallback execCb = nullptr);

  // Register internal handlers into the command map
  void RegisterHandlers();

  // handlers are implemented as free functions in handlers:: namespace

  // internal command handler map: cmd -> handler()
  std::unordered_map<int, std::function<int()>> m_cmdHandlers;

  // command handlers
  int HandleMakeDriverInfo();
  int HandleMakeDirectoryInfo();
  int HandleRunFile();
  int HandleDeleteFile();
  int HandleDownloadFile();
  int HandleMouseEvent();
  int HandleTestConnect();
};
