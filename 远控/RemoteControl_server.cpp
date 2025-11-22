// 远控.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "RemoteControl_server.h"
#include "../RemoteControl_Server/include/core/Enities.h"
#include "ServerSocket.h"
#include "framework.h"
#include "direct.h"
#include "io.h"
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

int MakeDriverInfo() {
  std::string result;

  // 使用位掩码一次性枚举 A:~Z:
  DWORD mask = GetLogicalDrives();
  for (int i = 0; i < 26; ++i) {
    if (mask & (1u << i)) {
      if (!result.empty())
        result += ",";
      result += char('A' + i);
    }
  }

  Cpacket pack(1, reinterpret_cast<const BYTE *>(result.data()), result.size());
  Dump(reinterpret_cast<const BYTE *>(pack.Data()), pack.Size());

  if (!CServerSocket::GetInstance().Send(pack)) {
    OutputDebugString(_T("发送驱动器信息失败"));
    return -1;
  }
  return 0;
}

int MakeDirectoryInfo() {
  std::string path;
  std::list<FILEINFO> listFileInfo;
  if (CServerSocket::GetInstance().GetFilePath(path) == false) {
    OutputDebugString(_T("当前的命令不是文件获取列表，命令解析错误"));
    return -1;
  }
  if (_chdir(path.c_str()) != 0) {
    FILEINFO finfo;
    finfo.isValid = true;
    finfo.isDir = true;
    finfo.hasNext = false;
    memcpy(finfo.szFileName, path.c_str(), path.size());
    // listFileInfo.push_back(finfo);
    Cpacket pack(2, (BYTE *)&finfo, sizeof(finfo));
    CServerSocket::GetInstance().Send(pack);
    OutputDebugString(_T("没有权限，访问目录"));
    return -2;
  }

  _finddata_t fdata;
  int hfind = 0;
  if ((hfind = _findfirst("*", &fdata)) == -1) {
    OutputDebugString(_T("没有找到任何文件"));
    return -3;
  }

  do {
    if (strcmp(fdata.name, ".") == 0 || strcmp(fdata.name, "..") == 0)
      continue;
    FILEINFO finfo;
    finfo.isValid = true;
    finfo.isDir = (fdata.attrib & _A_SUBDIR) != 0;
    memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));
    // listFileInfo.push_back(finfo);
    Cpacket pack(2, (BYTE *)&finfo, sizeof(finfo));
    CServerSocket::GetInstance().Send(pack);
    // 返回值处理？
  } while (!_findnext(hfind, &fdata));

  FILEINFO finfo;
  finfo.isValid = true;
  finfo.hasNext = false;

  Cpacket pack(2, (BYTE *)&finfo, sizeof(finfo));
  CServerSocket::GetInstance().Send(pack);

  return 0;
}

int RunFile() {
  std::string path;
  CServerSocket::GetInstance().GetFilePath(path);
  ShellExecuteA(NULL, NULL, path.c_str(), NULL, NULL, SW_SHOWNORMAL);
  Cpacket pack(3, NULL, 0);
  CServerSocket::GetInstance().Send(pack);
  return 0;
}

int DeleteFile() {
  std::string path;
  if (!CServerSocket::GetInstance().GetFilePath(path)) {
    OutputDebugString(_T("DeleteFile: 获取路径失败"));
    Cpacket err(CMD::CMD_ERROR, NULL, 0);
    CServerSocket::GetInstance().Send(err);
    return -1;
  }

  // 尝试删除文件
  int ret = remove(path.c_str());
  if (ret == 0) {
    // 发送删除成功回执（命令号 11）
    Cpacket ok(CMD::CMD_DELETE_FILE, NULL, 0);
    CServerSocket::GetInstance().Send(ok);
    return 0;
  } else {
    // 删除失败，发送错误回执并记录原因
    CStringA msgA;
    msgA.Format("DeleteFile failed, errno=%d", errno);
    OutputDebugStringA(msgA);
    Cpacket err(CMD::CMD_ERROR, (BYTE *)msgA.GetString(),
                (int)strlen(msgA.GetString()));
    CServerSocket::GetInstance().Send(err);
    return -2;
  }
}

int DownLoadFile() {

  std::string path;
  CServerSocket::GetInstance().GetFilePath(path);
  FILE *fp = nullptr;
  if (fopen_s(&fp, path.c_str(), "rb") != 0 || fp == NULL) {
    OutputDebugString(_T("打开文件失败"));
    // 发送错误回包（空数据） 使用 CMD_DOWNLOAD_FILE 发送失败标志
    Cpacket errpack(CMD::CMD_DOWNLOAD_FILE, NULL, 0);
    CServerSocket::GetInstance().Send(errpack);
    return -1;
  }

  // 获取文件大小
  _fseeki64(fp, 0, SEEK_END);
  long long fileSize = _ftelli64(fp);
  _fseeki64(fp, 0, SEEK_SET);

  // 发送文件大小包（客户端预期首包为 CMD_DOWNLOAD_FILE，包含文件大小）
  unsigned char sizeBuf[sizeof(fileSize)];
  memcpy(sizeBuf, &fileSize, sizeof(fileSize));
  Cpacket head(CMD::CMD_DOWNLOAD_FILE, (BYTE *)sizeBuf, sizeof(fileSize));
  CServerSocket::GetInstance().Send(head);

  const size_t CHUNK = 4096;
  std::vector<char> buffer(CHUNK);
  size_t nRead = 0;
  while ((nRead = fread(buffer.data(), 1, CHUNK, fp)) > 0) {
    Cpacket pack(CMD::CMD_DOWNLOAD_FILE, (BYTE *)buffer.data(), nRead);
    if (!CServerSocket::GetInstance().Send(pack)) {
      OutputDebugString(_T("发送文件数据失败"));
      break;
    }
  }
  fclose(fp);

  // 发送结束标志包
  Cpacket eofpack(CMD::CMD_EOF, NULL, 0);
  CServerSocket::GetInstance().Send(eofpack);

  return 0;
}

int MouseEvent() {
  MOUSEEVENT mouse;
  if (CServerSocket::GetInstance().GetMouseEvent(mouse)) {
    OutputDebugString(_T("获取鼠标事件成功"));

    DWORD nFlags = 0; // 鼠标标志位， 1 = 左键 2 = 右键 4 = 中键

    switch (mouse.nButton) {
    case 0: // 左键
      nFlags = 1;
      break;
    case 1: // 右键
      nFlags = 2;
      break;
    case 2: // 中键
      nFlags = 4;
      break;
    case 3: // 没有按键，移动
      nFlags = 8;
      break;
    default:
      break;
    }

    if (nFlags != 8)
      SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);

    switch (mouse.nAction) {
    case 0: // 单击
      nFlags |= 0x10;
      break;
    case 1: // 双击
      nFlags |= 0x20;
      break;
    case 2: // 按下
      nFlags |= 0x40;
      break;
    case 3: // 放开
      nFlags |= 0x80;
      break;
    default:
      break;
    }

    switch (nFlags) {
    case 0x11: // 左键单击
      mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x21: // 左键双击
      mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x41: // 左键按下
      mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x81: // 左键放开
      mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x12: // 右键单击
      mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x22: // 右键双击
      mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x42: // 右键按下
      mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x82: // 右键放开
      mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x14: // 中键单击
      mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x24: // 中键双击
      mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x44: // 中键按下
      mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x84: // 中键放开
      mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x08: // 鼠标移动
      mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0,
                  GetMessageExtraInfo());
      break;
    default:
      break;
    }

    Cpacket pack(5, (BYTE *)&mouse, sizeof(mouse));
    CServerSocket::GetInstance().Send(pack); // 发送回执
  } else {
    OutputDebugString(_T("获取鼠标事件失败"));
    return -1;
  }
}
#include "LockDialog.h"
CLockDialog dlg;
unsigned threadId = 0;

unsigned __stdcall threadLockDlg(void *arg) {
  dlg.Create(IDD_DIALOG_INFO);
  dlg.ShowWindow(SW_SHOW);
  // 窗口置顶
  dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
  // 限制鼠标功能
  ShowCursor(false);
  CRect rect;
  // 限制鼠标位置
  dlg.GetWindowRect(&rect);
  ClipCursor(&rect); // 限制鼠标在对话框范围内

  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    if (msg.message == WM_KEYDOWN) {
      break;
    }
  }
  ClipCursor(NULL); // 释放鼠标限制
  dlg.DestroyWindow();

  ShowCursor(true);
  _endthread();
  _endthreadex(0);
  return 0;
}

int LockMachine() {
  // 非模态
  if ((dlg.m_hWnd == NULL) ||
      (dlg.m_hWnd == INVALID_HANDLE_VALUE)) { // 判断窗口是否存在
    //_beginthread(threadLockDlg, 0, NULL);//如果不存在则创建线程显示窗口
    _beginthreadex(NULL, 0, threadLockDlg, NULL, 0, &threadId);
  }

  Cpacket pack(7, NULL, 0);                // 发送锁定命令
  CServerSocket::GetInstance().Send(pack); // 发送回执

  return 0;
}

int UnlockMachine() {
  PostThreadMessage(threadId, WM_KEYDOWN, 0,
                    0); // 向锁定线程发送消息，结束锁定对话框
  Cpacket pack(8, NULL, 0); // 发送回执
  CServerSocket::GetInstance().Send(pack);
  return 0;
}

int TestConnect() {
  Cpacket pack(2002, NULL, 0); // 发送回执
  TRACE("测试连接命令收到，发送回执\n");
  bool ret = CServerSocket::GetInstance().Send(pack);
  TRACE("测试连接命令回执发送结果： %d\n", ret);
  return 0;
}
int sendScreen() {
  CImage screen;
  HDC hScreen = ::GetDC(NULL);
  if (!hScreen) {
    OutputDebugString(_T("sendScreen: GetDC(NULL) failed"));
    return -1;
  }
  int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);
  int nWidth = GetDeviceCaps(hScreen, HORZRES);
  int nHeight = GetDeviceCaps(hScreen, VERTRES);
  screen.Create(nWidth, nHeight, nBitPerPixel);
  BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);
  ReleaseDC(NULL, hScreen);

  IStream *pStream = NULL;
  HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, &pStream);
  if (FAILED(hr) || pStream == NULL) {
    OutputDebugString(_T("sendScreen: CreateStreamOnHGlobal failed"));
    if (pStream)
      pStream->Release();
    screen.ReleaseDC();
    return -1;
  }

  // 保存为 PNG 到内存流
  if (screen.Save(pStream, Gdiplus::ImageFormatPNG) != S_OK) {
    OutputDebugString(_T("sendScreen: Save to PNG failed"));
    pStream->Release();
    screen.ReleaseDC();
    return -1;
  }

  // 获取流对应的 HGLOBAL
  HGLOBAL hGlobal = NULL;
  if (GetHGlobalFromStream(pStream, &hGlobal) != S_OK || hGlobal == NULL) {
    OutputDebugString(_T("sendScreen: GetHGlobalFromStream failed"));
    pStream->Release();
    screen.ReleaseDC();
    return -1;
  }

  SIZE_T nsize = GlobalSize(hGlobal);
  if (nsize == 0) {
    OutputDebugString(_T("sendScreen: GlobalSize returned 0"));
    pStream->Release();
    screen.ReleaseDC();
    return -1;
  }

  BYTE *pData = (BYTE *)GlobalLock(hGlobal);
  if (pData == NULL) {
    OutputDebugString(_T("sendScreen: GlobalLock failed"));
    pStream->Release();
    screen.ReleaseDC();
    return -1;
  }

  // 发送包（命令 6 用于屏幕数据）
  Cpacket pack(6, pData, (size_t)nsize);
  bool bRet = CServerSocket::GetInstance().Send(pack);
  CStringA msg;
  msg.Format("sendScreen: sent %zu bytes, success=%d\n", (size_t)nsize, bRet);
  OutputDebugStringA(msg);

  GlobalUnlock(hGlobal);
  pStream->Release();
  // GlobalFree(hGlobal); // 不显式释放由 CreateStreamOnHGlobal 分配的 HGLOBAL
  // when TRUE, stream owns it
  screen.ReleaseDC();

  return bRet ? 0 : -1;
}

// 屏幕流控制
static std::thread g_screenThread;
static std::atomic<bool> g_screenRunning{false};

void StartScreenStream(int intervalMs = 100) {
  if (g_screenRunning.load())
    return;
  g_screenRunning.store(true);
  g_screenThread = std::thread([intervalMs]() {
    while (g_screenRunning.load()) {
      sendScreen();
      std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
  });
}

void StopScreenStream() {
  if (!g_screenRunning.load())
    return;
  g_screenRunning.store(false);
  if (g_screenThread.joinable())
    g_screenThread.join();
}

int ExcuteCommand(int nCmd) {
  int ret = 0;
  Cpacket *pPacket = &CServerSocket::GetInstance().GetPacket();
  if (pPacket == nullptr)
    return -1;

  switch (nCmd) {
  case 1:
    ret = MakeDriverInfo();
    break;
  case 2:
    ret = MakeDirectoryInfo();
    break;
  case 3:
    ret = RunFile();
    break;
  case 4:
    ret = DownLoadFile(); // 下载文件
    break;
  case 11:
    ret = DeleteFile();
    break;
  case 5:
    ret = MouseEvent();
    break;
  case 6: // 发送屏幕内容(发送屏幕截图)
    ret = sendScreen();
    break;
  case 10:                  // 开始屏幕流
    StartScreenStream(100); // 默认 100ms 间隔（约10fps），可调整
    ret = 0;
    break;
  case 13: // 停止屏幕流
    StopScreenStream();
    ret = 0;
    break;
  case 7: // 锁定机器
    ret = LockMachine();
    // Sleep(500);
    // LockMachine();
    break;
  case 8: // 解锁机器
    ret = UnlockMachine();
    break;
  case 2002:
    ret = TestConnect();
  default:
    break;
  }
  return ret;
}
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

      // TODO: 在此处为应用程序的行为编写代码。
      CServerSocket &serverSocket = CServerSocket::GetInstance();
      int count = 0;
      if (!serverSocket.initSocket()) {
        wprintf(L"错误: 初始化套接字失败\n");
        return -1;
      }
      wprintf(L"服务器启动，等待客户端连接...\n");
      while (true) {
        if (serverSocket.AcceptClient() == false) {
          if (count >= 3) {
            wprintf(L"错误: 客户端连接失败超过3次，程序退出。\n");
            exit(0);
          }
          wprintf(L"等待客户端连接失败，重试中...\n");
          count++;
          continue;
        }
        TRACE("客户端连接成功\n");
        while (true) {
          int cmd = serverSocket.DealCommand();
          if (cmd > 0) {
            TRACE("收到命令: %d\r\n", cmd);
            ExcuteCommand(cmd);
            continue;
          }
          wprintf(L"错误: 处理命令失败\n");
          break;
        }
        serverSocket.CloseClient();
      }
    }
  } else {
    // TODO: 更改错误代码以符合需要
    wprintf(L"错误: GetModuleHandle 失败\n");
    nRetCode = 1;
  }

  return nRetCode;
}
