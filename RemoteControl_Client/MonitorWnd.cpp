#include "pch.h"
#include "MonitorWnd.h"
#include "Enities.h"
#include "clientSocket.h"
#include <cmath>
#include <direct.h>
#include <gdiplus.h>


// forward declaration for static subclass proc
static LRESULT CALLBACK MonitorStatic_WndProc(HWND hwnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam);

// Resource IDs come from `Resource.h`.

IMPLEMENT_DYNAMIC(CMonitorWnd, CDialog)

BEGIN_MESSAGE_MAP(CMonitorWnd, CDialog)
ON_WM_PAINT()
ON_WM_ERASEBKGND()
ON_WM_DESTROY()
ON_MESSAGE(WM_UPDATE_SCREEN, &CMonitorWnd::OnUpdateScreen)
ON_BN_CLICKED(IDC_CHECK_MOUSE_CONTROL,
              &CMonitorWnd::OnBnClickedCheckMouseControl)
ON_BN_CLICKED(IDC_BTN_LOCK, &CMonitorWnd::OnBnClickedBtnLock)
ON_BN_CLICKED(IDC_BTN_UNLOCK, &CMonitorWnd::OnBnClickedBtnUnlock)
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_RBUTTONDOWN()
ON_WM_RBUTTONUP()
ON_WM_MBUTTONDOWN()
ON_WM_MBUTTONUP()
END_MESSAGE_MAP()

CMonitorWnd::CMonitorWnd(CWnd *pParent /*=nullptr*/)
    : CDialog(IDD_MONITOR_DIALOG, pParent) {}

CMonitorWnd::~CMonitorWnd() { StopMonitor(); }

bool CMonitorWnd::CreateMonitorWindow(CWnd *pParent, CClientSocket *pSocket) {
  if (!pParent)
    return false;
  m_pSocket = pSocket;

  // Initialize GDI+ if needed (safe to call repeatedly)
  static ULONG_PTR s_gdiplusToken = 0;
  if (s_gdiplusToken == 0) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&s_gdiplusToken, &gdiplusStartupInput, nullptr);
  }

  if (!CDialog::Create(IDD_MONITOR_DIALOG, pParent)) {
    return false;
  }

  // Register this window to receive screen updates and request start
  if (m_pSocket) {
    m_pSocket->SetScreenViewWnd(GetSafeHwnd());
    Cpacket pkt(REQ_START_WATCH, NULL, 0);
    m_pSocket->Send(pkt);
    // start receiving loop
    StartReceiveLoop();
  } else {
    CClientSocket::GetInstance().SetScreenViewWnd(GetSafeHwnd());
    Cpacket pkt(REQ_START_WATCH, NULL, 0);
    CClientSocket::GetInstance().Send(pkt);
    m_pSocket = &CClientSocket::GetInstance();
    StartReceiveLoop();
  }

  m_remoteCanvasW = 0;
  m_remoteCanvasH = 0;

  m_bIsMonitoring = true;
  m_thread = std::thread(&CMonitorWnd::MonitorThreadFunc, this);
  return true;
}

void CMonitorWnd::StopMonitor() {
  m_bIsMonitoring = false;
  if (m_thread.joinable())
    m_thread.join();
  if (m_canvas)
    m_canvas.Destroy();
}

void CMonitorWnd::OnBnClickedBtnLock() {
  m_bMouseControlEnabled = false;
  try {
    Cpacket pkt(CMD::CMD_LOCK_MACHINE, NULL, 0);
    bool ok = false;
    if (m_pSocket)
      ok = m_pSocket->Send(pkt);
    else
      ok = CClientSocket::GetInstance().Send(pkt);
    if (!ok) {
      OutputDebugString(_T("OnBnClickedBtnLock: Failed to send lock command\n"));
    }
  } catch (...) {
    OutputDebugString(_T("OnBnClickedBtnLock: Exception sending lock command\n"));
  }
}

void CMonitorWnd::OnBnClickedBtnUnlock() {
  m_bMouseControlEnabled = true;
  try {
    Cpacket pkt(CMD::CMD_UNLOCK_MACHINE, NULL, 0);
    bool ok = false;
    if (m_pSocket)
      ok = m_pSocket->Send(pkt);
    else
      ok = CClientSocket::GetInstance().Send(pkt);
    if (!ok) {
      OutputDebugString(_T("OnBnClickedBtnUnlock: Failed to send unlock command\n"));
    }
  } catch (...) {
    OutputDebugString(_T("OnBnClickedBtnUnlock: Exception sending unlock command\n"));
  }
}
void CMonitorWnd::OnBnClickedCheckMouseControl() {
  CButton *pBtn = (CButton *)GetDlgItem(IDC_CHECK_MOUSE_CONTROL);
  if (pBtn)
    m_bMouseControlEnabled = (pBtn->GetCheck() == BST_CHECKED);
}

bool CMonitorWnd::MapClientPointToRemote(const CPoint &clientPt,
                                         CPoint &outRemotePt) {
  // Determine the client area where the remote canvas is drawn (monitor static
  // control)
  CRect monitorRc;
  CWnd *pStatic = GetDlgItem(IDC_STATIC_MONITOR);
  if (pStatic && pStatic->GetSafeHwnd()) {
    pStatic->GetWindowRect(&monitorRc);
    ScreenToClient(&monitorRc);
  } else {
    // fallback to full client area
    GetClientRect(&monitorRc);
  }
  int clientW = monitorRc.Width();
  int clientH = monitorRc.Height();

  int canvasW = 0, canvasH = 0;
  {
    std::lock_guard<std::mutex> lock(m_canvasMutex);
    canvasW = (m_remoteCanvasW > 0) ? m_remoteCanvasW : m_canvas.GetWidth();
    canvasH = (m_remoteCanvasH > 0) ? m_remoteCanvasH : m_canvas.GetHeight();
  }

  if (canvasW <= 0 || canvasH <= 0 || clientW <= 0 || clientH <= 0)
    return false;

  double scale1 = (double)clientW / (double)canvasW;
  double scale2 = (double)clientH / (double)canvasH;
  double scale = (scale1 < scale2) ? scale1 : scale2;
  if (scale <= 0.0)
    return false;
  int destW = (int)floor(canvasW * scale + 0.5);
  if (destW < 1)
    destW = 1;
  int destH = (int)floor(canvasH * scale + 0.5);
  if (destH < 1)
    destH = 1;
  int destLeft = monitorRc.left + (clientW - destW) / 2;
  int destTop = monitorRc.top + (clientH - destH) / 2;

  // check if point is inside the drawn area
  if (clientPt.x < destLeft || clientPt.x >= destLeft + destW ||
      clientPt.y < destTop || clientPt.y >= destTop + destH)
    return false;

  double fx = (double)(clientPt.x - destLeft) / scale;
  double fy = (double)(clientPt.y - destTop) / scale;
  int rx = (int)floor(fx + 0.5);
  int ry = (int)floor(fy + 0.5);
  if (rx < 0)
    rx = 0;
  if (rx > canvasW - 1)
    rx = canvasW - 1;
  if (ry < 0)
    ry = 0;
  if (ry > canvasH - 1)
    ry = canvasH - 1;
  outRemotePt = CPoint(rx, ry);
  return true;
}

void CMonitorWnd::SendMouseEventToServer(WORD nButton, WORD nAction,
                                         const CPoint &remotePt) {
  try {
    // Use MOUSEEVENT structure (as used in clientSocket.h)
    MOUSEEVENT me;
    me.nAction = nAction;
    me.nButton = nButton;
    me.ptXY.x = remotePt.x;
    me.ptXY.y = remotePt.y;

    Cpacket pkt(CMD_MOUSE_EVENT, reinterpret_cast<const BYTE *>(&me),
                sizeof(MOUSEEVENT));
    bool ok = false;
    if (m_pSocket)
      ok = m_pSocket->Send(pkt);
    else
      ok = CClientSocket::GetInstance().Send(pkt);

    if (!ok) {
      OutputDebugString(_T("SendMouseEventToServer: Failed to send packet.\n"));
    } else {
      // Uncomment for verbose logging if needed
      // CString dbg; dbg.Format(_T("SendMouseEventToServer: Sent Action=%d
      // Button=%d Pt=(%d,%d)\n"), nAction, nButton, remotePt.x, remotePt.y);
      // OutputDebugString(dbg);
    }
  } catch (...) {
    OutputDebugString(_T("SendMouseEventToServer: Exception caught.\n"));
  }
}

void CMonitorWnd::OnMouseMove(UINT /*nFlags*/, CPoint point) {
  if (!m_bMouseControlEnabled)
    return;
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - m_lastMoveTime)
                     .count();
  if (elapsed < m_moveThrottleMs)
    return;
  m_lastMoveTime = now;
  CPoint remotePt;
  if (MapClientPointToRemote(point, remotePt)) {
    SendMouseEventToServer(0xFFFF, 4, remotePt); // move flag
  }
}

void CMonitorWnd::OnLButtonDown(UINT /*nFlags*/, CPoint point) {
  if (!m_bMouseControlEnabled)
    return;
  CPoint remotePt;
  if (MapClientPointToRemote(point, remotePt))
    SendMouseEventToServer(0 /*Left*/, 2 /*down*/, remotePt);
}

void CMonitorWnd::OnLButtonUp(UINT /*nFlags*/, CPoint point) {
  if (!m_bMouseControlEnabled)
    return;
  CPoint remotePt;
  if (MapClientPointToRemote(point, remotePt))
    SendMouseEventToServer(0 /*Left*/, 3 /*up*/, remotePt);
}

void CMonitorWnd::OnRButtonDown(UINT /*nFlags*/, CPoint point) {
  if (!m_bMouseControlEnabled)
    return;
  CPoint remotePt;
  if (MapClientPointToRemote(point, remotePt))
    SendMouseEventToServer(1 /*Right*/, 2 /*down*/, remotePt);
}

void CMonitorWnd::OnRButtonUp(UINT /*nFlags*/, CPoint point) {
  if (!m_bMouseControlEnabled)
    return;
  CPoint remotePt;
  if (MapClientPointToRemote(point, remotePt))
    SendMouseEventToServer(1 /*Right*/, 3 /*up*/, remotePt);
}

void CMonitorWnd::OnMButtonDown(UINT /*nFlags*/, CPoint point) {
  if (!m_bMouseControlEnabled)
    return;
  CPoint remotePt;
  if (MapClientPointToRemote(point, remotePt))
    SendMouseEventToServer(2 /*Middle*/, 2 /*down*/, remotePt);
}

void CMonitorWnd::OnMButtonUp(UINT /*nFlags*/, CPoint point) {
  if (!m_bMouseControlEnabled)
    return;
  CPoint remotePt;
  if (MapClientPointToRemote(point, remotePt))
    SendMouseEventToServer(2 /*Middle*/, 3 /*up*/, remotePt);
}

void CMonitorWnd::DoDataExchange(CDataExchange *pDX) {
  CDialog::DoDataExchange(pDX);
}

BOOL CMonitorWnd::OnInitDialog() {
  BOOL ok = CDialog::OnInitDialog();
  // reflect current mouse-control enabled state to the checkbox
  CButton *pBtn = (CButton *)GetDlgItem(IDC_CHECK_MOUSE_CONTROL);
  if (pBtn)
    pBtn->SetCheck(m_bMouseControlEnabled.load() ? BST_CHECKED : BST_UNCHECKED);
  // Ensure the monitor static control sends mouse messages
  CWnd *pStatic = GetDlgItem(IDC_STATIC_MONITOR);
  if (pStatic && pStatic->GetSafeHwnd()) {
    // add SS_NOTIFY so static control will generate clicks, then subclass it
    pStatic->ModifyStyle(0, SS_NOTIFY);
    pStatic->SetWindowPos(NULL, 0, 0, 0, 0,
                          SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                              SWP_FRAMECHANGED);

    HWND hStatic = pStatic->GetSafeHwnd();
    // Check if already subclassed to avoid double subclassing
    if (!GetProp(hStatic, _T("MonitorSubclassed"))) {
      // store original wndproc in a window property and replace with our proc
      LONG_PTR orig = SetWindowLongPtr(hStatic, GWLP_WNDPROC,
                                       (LONG_PTR)MonitorStatic_WndProc);
      SetProp(hStatic, _T("OrigStaticProc"), (HANDLE)orig);
      // Save a marker so we know we've subclassed it (cleanup in OnDestroy)
      SetProp(hStatic, _T("MonitorSubclassed"), (HANDLE)1);
      OutputDebugString(_T("CMonitorWnd::OnInitDialog: Subclassed ")
                        _T("IDC_STATIC_MONITOR successfully.\n"));
    }
  } else {
    OutputDebugString(
        _T("CMonitorWnd::OnInitDialog: Failed to find IDC_STATIC_MONITOR.\n"));
  }
  return ok;
}

// Subclass procedure helper to forward mouse messages from the static control
static LRESULT CALLBACK MonitorStatic_WndProc(HWND hwnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam) {
  // retrieve original proc and owner pointer from window properties
  WNDPROC orig = (WNDPROC)(LONG_PTR)GetProp(hwnd, _T("OrigStaticProc"));
  HWND hParent = ::GetParent(hwnd);
  switch (msg) {
  case WM_MOUSEMOVE: {
    // translate point to parent client coords and forward as WM_MOUSEMOVE
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ::MapWindowPoints(hwnd, hParent, &pt, 1);
    ::SendMessage(hParent, WM_MOUSEMOVE, 0, MAKELPARAM(pt.x, pt.y));
    break;
  }
  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
  case WM_RBUTTONDOWN:
  case WM_RBUTTONUP:
  case WM_MBUTTONDOWN:
  case WM_MBUTTONUP: {
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ::MapWindowPoints(hwnd, hParent, &pt, 1);
    // forward to parent with same message
    ::SendMessage(hParent, msg, wParam, MAKELPARAM(pt.x, pt.y));

    // Debug log for mouse clicks
    // CString dbg; dbg.Format(_T("MonitorStatic_WndProc: Forwarded msg=0x%X
    // pt=(%d,%d)\n"), msg, pt.x, pt.y); OutputDebugString(dbg);
    break;
  }
  }
  if (orig)
    return CallWindowProc(orig, hwnd, msg, wParam, lParam);
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void CMonitorWnd::OnDestroy() {
  // Send stop watch and unregister
  if (m_pSocket) {
    Cpacket pkt(REQ_STOP_WATCH, NULL, 0);
    m_pSocket->Send(pkt);
    m_pSocket->SetScreenViewWnd(NULL);
  } else {
    Cpacket pkt(REQ_STOP_WATCH, NULL, 0);
    CClientSocket::GetInstance().Send(pkt);
    CClientSocket::GetInstance().SetScreenViewWnd(NULL);
  }
  // Remove any pending WM_UPDATE_SCREEN messages for this window and free their
  // buffers
  MSG msg;
  while (PeekMessage(&msg, GetSafeHwnd(), WM_UPDATE_SCREEN, WM_UPDATE_SCREEN,
                     PM_REMOVE)) {
    if (msg.lParam) {
      char *p = (char *)msg.lParam;
      delete[] p;
    }
  }

  // stop receiving loop and monitor thread
  StopReceiveLoop();

  // restore subclassed static control if we replaced its WndProc
  CWnd *pStatic = GetDlgItem(IDC_STATIC_MONITOR);
  if (pStatic && pStatic->GetSafeHwnd()) {
    HWND hStatic = pStatic->GetSafeHwnd();
    if (GetProp(hStatic, _T("MonitorSubclassed"))) {
      // restore original wndproc
      WNDPROC orig = (WNDPROC)(LONG_PTR)GetProp(hStatic, _T("OrigStaticProc"));
      if (orig)
        SetWindowLongPtr(hStatic, GWLP_WNDPROC, (LONG_PTR)orig);
      RemoveProp(hStatic, _T("OrigStaticProc"));
      RemoveProp(hStatic, _T("MonitorSubclassed"));
    }
  }

  // close socket to ensure OS receive buffer is cleared so next start has fresh
  // state
  if (m_pSocket) {
    m_pSocket->CloseSocket();
  } else {
    CClientSocket::GetInstance().CloseSocket();
  }

  StopMonitor();
  CDialog::OnDestroy();
}

// OnBnClickedBtnExit removed; main dialog handles monitor stop now.

BOOL CMonitorWnd::OnEraseBkgnd(CDC * /*pDC*/) { return TRUE; }

void CMonitorWnd::OnPaint() {
  // Double-buffered painting to avoid flicker
  CPaintDC dc(this);
  CRect clientRc;
  GetClientRect(&clientRc);

  // Prepare memory DC and bitmap
  CDC memDC;
  memDC.CreateCompatibleDC(&dc);
  CBitmap bmp;
  bmp.CreateCompatibleBitmap(&dc, clientRc.Width(), clientRc.Height());
  CBitmap *oldBmp = memDC.SelectObject(&bmp);

  // fill background
  memDC.FillSolidRect(&clientRc, RGB(30, 30, 30));

  // find monitor static control and draw only inside it
  CWnd *pStatic = GetDlgItem(IDC_STATIC_MONITOR);
  CRect monitorRc;
  if (pStatic && pStatic->GetSafeHwnd()) {
    pStatic->GetWindowRect(&monitorRc);
    ScreenToClient(&monitorRc);
  } else {
    // fallback to center area
    monitorRc = CRect(8, 8, clientRc.right - 8, clientRc.bottom - 56);
  }

  std::lock_guard<std::mutex> lock(m_canvasMutex);
  if (m_canvas) {
    int canvasW = m_canvas.GetWidth();
    int canvasH = m_canvas.GetHeight();
    int viewW = monitorRc.Width();
    int viewH = monitorRc.Height();
    if (canvasW > 0 && canvasH > 0 && viewW > 0 && viewH > 0) {
      double scale1 = (double)viewW / (double)canvasW;
      double scale2 = (double)viewH / (double)canvasH;
      double scale = (scale1 < scale2) ? scale1 : scale2;
      int destW = (int)floor(canvasW * scale + 0.5);
      if (destW < 1)
        destW = 1;
      int destH = (int)floor(canvasH * scale + 0.5);
      if (destH < 1)
        destH = 1;
      int destLeft = monitorRc.left + (viewW - destW) / 2;
      int destTop = monitorRc.top + (viewH - destH) / 2;
      HDC hCanvasDC = m_canvas.GetDC();
      if (hCanvasDC) {
        m_canvas.Draw(memDC.GetSafeHdc(), destLeft, destTop, destW, destH, 0, 0,
                      canvasW, canvasH);
        m_canvas.ReleaseDC();
      }
    }
  }

  // Blit memDC to screen in one operation
  dc.BitBlt(0, 0, clientRc.Width(), clientRc.Height(), &memDC, 0, 0, SRCCOPY);

  // cleanup
  memDC.SelectObject(oldBmp);
  bmp.DeleteObject();
  memDC.DeleteDC();
}

LRESULT CMonitorWnd::OnUpdateScreen(WPARAM wParam, LPARAM lParam) {
  size_t nSize = (size_t)wParam;
  char *pData = (char *)lParam;
  if (pData == nullptr || nSize == 0)
    return 0;

  // Try to parse as Cpacket with png payload similar to ScreenViewDlg
  size_t pngOffset = 0;
  size_t pngSize = nSize;
  bool isCpacket = false;
  if (nSize >= 2 && *(WORD *)pData == 0xFEFF) {
    isCpacket = true;
    if (nSize >= 10) {
      DWORD packetLength = *(DWORD *)(pData + 2);
      if (packetLength >= 6 && packetLength <= 10 * 1024 * 1024) {
        size_t expectedTotal = 2 + 4 + packetLength;
        if (nSize >= expectedTotal) {
          pngOffset = 8;
          pngSize = packetLength - 4;
        } else {
          isCpacket = false;
          pngOffset = 0;
          pngSize = nSize;
        }
      } else {
        isCpacket = false;
      }
    } else {
      isCpacket = false;
    }
  }

  // Load into CImage
  HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, pngSize);
  if (!hMem) {
    delete[] pData;
    return 0;
  }
  void *pMem = GlobalLock(hMem);
  memcpy(pMem, pData + pngOffset, pngSize);
  GlobalUnlock(hMem);
  IStream *pStream = nullptr;
  if (FAILED(CreateStreamOnHGlobal(hMem, TRUE, &pStream))) {
    GlobalFree(hMem);
    delete[] pData;
    return 0;
  }
  HRESULT hr = S_OK;
  {
    std::lock_guard<std::mutex> lock(m_canvasMutex);
    if (!m_canvas.IsNull())
      m_canvas.Destroy();
    hr = m_canvas.Load(pStream);
    if (SUCCEEDED(hr)) {
      // update remote canvas size
      m_remoteCanvasW = m_canvas.GetWidth();
      m_remoteCanvasH = m_canvas.GetHeight();
    }
  }
  pStream->Release();
  // trigger redraw
  if (IsWindow(GetSafeHwnd()))
    Invalidate(FALSE);

  delete[] pData;
  return 0;
}

void CMonitorWnd::MonitorThreadFunc() {
  while (m_bIsMonitoring) {
    // Minimal loop to keep thread alive; real implementation fetches screen
    // frames
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void CMonitorWnd::SaveFrameToDisk() {
  // create recordings directory if not exists
  if (_mkdir("recordings") != 0) {
    // ignore error if already exists
  }
  // Save current canvas if any
  std::lock_guard<std::mutex> lock(m_canvasMutex);
  if (m_canvas) {
    CString cpath;
    cpath.Format(_T("recordings\\frame_%d.png"), m_frameIndex++);
    m_canvas.Save(cpath);
  }
}

void CMonitorWnd::StartReceiveLoop() {
  if (m_pSocket == nullptr || m_receiving.load(std::memory_order_relaxed))
    return;
  m_receiving.store(true, std::memory_order_release);
  m_receiveThread = std::thread([this]() {
    while (m_receiving.load(std::memory_order_acquire)) {
      int cmd = m_pSocket->DealCommand(100, &m_receiving);
      if (cmd <= 0)
        break;
      if (cmd != SCREEN_DATA_PACKET) {
        // unexpected, but continue
        continue;
      }
    }
    m_receiving.store(false, std::memory_order_release);
  });
}

void CMonitorWnd::StopReceiveLoop() {
  m_receiving.store(false, std::memory_order_release);
  if (m_receiveThread.joinable())
    m_receiveThread.join();
}
