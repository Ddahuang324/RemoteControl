#include "pch.h"
#include "ScreenStreamer.h"
#include "ServerSocket.h"
#include <atlimage.h>
#include <objidl.h>
#include <windows.h>
#include <chrono>
#include <thread>
#include <vector>

ScreenStreamer::ScreenStreamer() : m_running(false) {}

ScreenStreamer::~ScreenStreamer() {
  StopStream();
}

int ScreenStreamer::SendOnce() {
  CImage screen;
  HDC hScreen = ::GetDC(NULL);
  if (!hScreen) {
    OutputDebugString(_T("ScreenStreamer::SendOnce: GetDC(NULL) failed"));
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
    OutputDebugString(_T("ScreenStreamer::SendOnce: CreateStreamOnHGlobal failed"));
    if (pStream)
      pStream->Release();
    screen.ReleaseDC();
    return -1;
  }

  if (screen.Save(pStream, Gdiplus::ImageFormatPNG) != S_OK) {
    OutputDebugString(_T("ScreenStreamer::SendOnce: Save to PNG failed"));
    pStream->Release();
    screen.ReleaseDC();
    return -1;
  }

  HGLOBAL hGlobal = NULL;
  if (GetHGlobalFromStream(pStream, &hGlobal) != S_OK || hGlobal == NULL) {
    OutputDebugString(_T("ScreenStreamer::SendOnce: GetHGlobalFromStream failed"));
    pStream->Release();
    screen.ReleaseDC();
    return -1;
  }

  SIZE_T nsize = GlobalSize(hGlobal);
  if (nsize == 0) {
    OutputDebugString(_T("ScreenStreamer::SendOnce: GlobalSize returned 0"));
    pStream->Release();
    screen.ReleaseDC();
    return -1;
  }

  BYTE *pData = (BYTE *)GlobalLock(hGlobal);
  if (pData == NULL) {
    OutputDebugString(_T("ScreenStreamer::SendOnce: GlobalLock failed"));
    pStream->Release();
    screen.ReleaseDC();
    return -1;
  }

  Cpacket pack(6, pData, (size_t)nsize);
  bool bRet = CServerSocket::GetInstance().Send(pack);
  CStringA msg;
  msg.Format("ScreenStreamer::SendOnce: sent %zu bytes, success=%d\n", (size_t)nsize, bRet);
  OutputDebugStringA(msg);

  GlobalUnlock(hGlobal);
  pStream->Release();
  screen.ReleaseDC();

  return bRet ? 0 : -1;
}

void ScreenStreamer::ThreadLoop(int intervalMs) {
  while (m_running.load()) {
    SendOnce();
    std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
  }
}

void ScreenStreamer::StartStream(int intervalMs) {
  if (m_running.load())
    return;
  m_running.store(true);
  m_thread = std::thread(&ScreenStreamer::ThreadLoop, this, intervalMs);
}

void ScreenStreamer::StopStream() {
  if (!m_running.load())
    return;
  m_running.store(false);
  if (m_thread.joinable())
    m_thread.join();
}

bool ScreenStreamer::IsRunning() const { return m_running.load(); }
