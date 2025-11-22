#include "pch.h"
// encoding: UTF-8
#include "Enities.h"
#include "MonitorWnd.h"
#include <algorithm>
#include <atlstr.h>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

// GDI+ 用于最近邻采样（避免缩放模糊）
#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment(lib, "gdiplus.lib")

IMPLEMENT_DYNAMIC(CMonitorWnd, CDialog)

BEGIN_MESSAGE_MAP(CMonitorWnd, CDialog)
ON_WM_PAINT()
ON_WM_ERASEBKGND()
ON_WM_DESTROY()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_RBUTTONDOWN()
ON_WM_RBUTTONUP()
ON_WM_MBUTTONDOWN()
ON_WM_MBUTTONUP()
ON_BN_CLICKED(IDC_BTN_LOCK, &CMonitorWnd::OnBnClickedBtnLock)
ON_BN_CLICKED(IDC_BTN_UNLOCK, &CMonitorWnd::OnBnClickedBtnUnlock)
ON_BN_CLICKED(IDC_CHECK_MOUSE_CONTROL, &CMonitorWnd::OnBnClickedCheckMouseControl)
END_MESSAGE_MAP()

CMonitorWnd::CMonitorWnd(CWnd *pParent /*=nullptr*/) : CDialog(IDD_MONITOR_DIALOG, pParent) {}

CMonitorWnd::~CMonitorWnd() { StopMonitor(); }

bool CMonitorWnd::CreateMonitorWindow(CWnd *pParent, CclientSocket *pSocket) {
  if (!pParent)
    return false;
  m_pSocket = pSocket;

  // 确保 GDI+ 已初始化（用于使用最近邻插值绘制）
  static ULONG_PTR s_gdiplusToken = 0;
  if (s_gdiplusToken == 0) {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&s_gdiplusToken, &gdiplusStartupInput, nullptr);
  }

  // 使用对话框资源创建模式/无模态对话框，以便使用资源编辑器中新建的布局
  if (!CDialog::Create(IDD_MONITOR_DIALOG, pParent)) {
    return false;
  }

  // 确保标题与资源一致
  SetWindowText(_T("屏幕监视"));

  // 创建全屏大小的画布用于合成（以屏幕分辨率为准）
  // 不要在客户端使用本机分辨率创建画布。
  // 画布将在收到服务端首帧时根据远端分辨率创建，以保证映射基准一致。
  m_remoteCanvasW = 0;
  m_remoteCanvasH = 0;

  // 启动后台线程
  m_bIsMonitoring = true;
  m_thread = std::thread(&CMonitorWnd::MonitorThreadFunc, this);
  return true;
}

// 简单的按钮事件处理：更新鼠标控制状态
void CMonitorWnd::OnBnClickedBtnLock()
{
  // 发送锁机命令到服务端
  if (m_pSocket) {
    try {
      Cpacket pkt(CMD_LOCK_MACHINE, {});
      m_pSocket->SendPacket(pkt);
    } catch (...) {
      // 忽略发送错误
    }
  }

  // 更新本地状态：禁止鼠标控制并更新 UI
  m_bMouseControlEnabled = false;
  CWnd *pLock = GetDlgItem(IDC_BTN_LOCK);
  CWnd *pUnlock = GetDlgItem(IDC_BTN_UNLOCK);
  if (pLock)
    pLock->EnableWindow(FALSE);
  if (pUnlock)
    pUnlock->EnableWindow(TRUE);
  CButton* pChk = (CButton*)GetDlgItem(IDC_CHECK_MOUSE_CONTROL);
  if (pChk) {
    pChk->SetCheck(BST_UNCHECKED);
  }
}

void CMonitorWnd::OnBnClickedBtnUnlock()
{
  // 发送解锁命令到服务端
  if (m_pSocket) {
    try {
      Cpacket pkt(CMD_UNLOCK_MACHINE, {});
      m_pSocket->SendPacket(pkt);
    } catch (...) {
      // 忽略发送错误
    }
  }

  // 更新本地状态：允许鼠标控制并更新 UI
  m_bMouseControlEnabled = true;
  CWnd *pLock = GetDlgItem(IDC_BTN_LOCK);
  CWnd *pUnlock = GetDlgItem(IDC_BTN_UNLOCK);
  if (pLock)
    pLock->EnableWindow(TRUE);
  if (pUnlock)
    pUnlock->EnableWindow(FALSE);
  CButton* pChk = (CButton*)GetDlgItem(IDC_CHECK_MOUSE_CONTROL);
  if (pChk) {
    pChk->SetCheck(BST_CHECKED);
  }
}

void CMonitorWnd::OnBnClickedCheckMouseControl()
{
  CButton* pBtn = (CButton*)GetDlgItem(IDC_CHECK_MOUSE_CONTROL);
  if (pBtn) {
    m_bMouseControlEnabled = (pBtn->GetCheck() == BST_CHECKED);
  }
}

// 把 client 窗口坐标映射到远端 canvas 像素坐标（若在 letterbox 外返回 false）
bool CMonitorWnd::MapClientPointToRemote(const CPoint &clientPt,
                                         CPoint &outRemotePt) {
  CRect rc;
  GetClientRect(&rc);
  int clientW = rc.Width();
  int clientH = rc.Height();

  int canvasW = 0;
  int canvasH = 0;
  {
    std::lock_guard<std::mutex> lock(m_canvasMutex);
    canvasW = (m_remoteCanvasW > 0) ? m_remoteCanvasW : m_canvas.GetWidth();
    canvasH = (m_remoteCanvasH > 0) ? m_remoteCanvasH : m_canvas.GetHeight();
  }

  if (canvasW <= 0 || canvasH <= 0 || clientW <= 0 || clientH <= 0)
    return false;

  double scale = (std::min)((double)clientW / (double)canvasW,
                            (double)clientH / (double)canvasH);
  if (scale <= 0.0)
    return false;

  int destW = (std::max)(1, (int)std::lround(canvasW * scale));
  int destH = (std::max)(1, (int)std::lround(canvasH * scale));
  int destLeft = (clientW - destW) / 2;
  int destTop = (clientH - destH) / 2;

  // 是否在显示区域（非 letterbox）
  if (clientPt.x < destLeft || clientPt.x >= destLeft + destW ||
      clientPt.y < destTop || clientPt.y >= destTop + destH) {
    return false;
  }

  double fx = (double)(clientPt.x - destLeft) / scale;
  double fy = (double)(clientPt.y - destTop) / scale;

  int rx = (int)std::lround(fx);
  int ry = (int)std::lround(fy);

  rx = std::clamp(rx, 0, canvasW - 1);
  ry = std::clamp(ry, 0, canvasH - 1);

  outRemotePt = CPoint(rx, ry);
  return true;
}

// 封装并发送鼠标事件到服务端
void CMonitorWnd::SendMouseEventToServer(WORD nButton, WORD nAction,
                                         const CPoint &remotePt) {
  if (!m_pSocket)
    return;
  try {
    MouseEventData me;
    me.nAction = nAction;
    me.nButton = nButton;
    me.ptXY.x = remotePt.x;
    me.ptXY.y = remotePt.y;

    std::vector<BYTE> data = me.Serialize();
    Cpacket pkt(CMD_MOUSE_EVENT, data);
    m_pSocket->SendPacket(pkt);
  } catch (...) {
    // 忽略发送错误
  }
}

void CMonitorWnd::OnMouseMove(UINT /*nFlags*/, CPoint point) {
  if (!m_bMouseControlEnabled)
    return;

  // 节流：只每 m_moveThrottleMs 发送一次移动事件
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - m_lastMoveTime)
                     .count();
  if (elapsed < m_moveThrottleMs)
    return;
  m_lastMoveTime = now;

  CPoint remotePt;
  if (MapClientPointToRemote(point, remotePt)) {
    // Use nButton=0xFFFF as move flag, nAction=4 (Move)
    SendMouseEventToServer(0xFFFF, 4, remotePt);
  }
}

void CMonitorWnd::DoDataExchange(CDataExchange *pDX) {
  CDialog::DoDataExchange(pDX);
}

BOOL CMonitorWnd::OnInitDialog() {
  BOOL ok = CDialog::OnInitDialog();
  // 初始 UI 状态：默认未允许鼠标控制
  CButton* pChk = (CButton*)GetDlgItem(IDC_CHECK_MOUSE_CONTROL);
  if (pChk) {
    pChk->SetCheck(m_bMouseControlEnabled ? BST_CHECKED : BST_UNCHECKED);
  }
  CWnd *pLock = GetDlgItem(IDC_BTN_LOCK);
  CWnd *pUnlock = GetDlgItem(IDC_BTN_UNLOCK);
  if (pLock)
    pLock->EnableWindow(TRUE);
  if (pUnlock)
    pUnlock->EnableWindow(FALSE);

  return ok;
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
  if (MapClientPointToRemote(point, remotePt)) {
    SendMouseEventToServer(0 /*Left*/, 3 /*up*/, remotePt);
  }
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
  if (MapClientPointToRemote(point, remotePt)) {
    SendMouseEventToServer(1 /*Right*/, 3 /*up*/, remotePt);
  }
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
  if (MapClientPointToRemote(point, remotePt)) {
    SendMouseEventToServer(2 /*Middle*/, 3 /*up*/, remotePt);
  }
}

void CMonitorWnd::StopMonitor() {
  m_bIsMonitoring = false;
  if (m_thread.joinable())
    m_thread.join();
  if (m_canvas)
    m_canvas.Destroy();
}

void CMonitorWnd::OnDestroy() {
  StopMonitor();
  CWnd::OnDestroy();
}

BOOL CMonitorWnd::OnEraseBkgnd(CDC * /*pDC*/) {
  // 返回 TRUE，告诉系统我们已经处理背景擦除（实际在 OnPaint
  // 中双缓冲绘制），以避免闪烁
  return TRUE;
}

void CMonitorWnd::OnPaint() {
  CPaintDC dc(this);
  std::lock_guard<std::mutex> lock(m_canvasMutex);
  CRect rc;
  GetClientRect(&rc);
  int clientW = rc.Width();
  int clientH = rc.Height();

  // 双缓冲：在内存 DC 上先绘制（包含 letterbox 背景），然后一次性 blit
  // 到窗口，避免闪烁
  CDC memDC;
  memDC.CreateCompatibleDC(&dc);
  CBitmap bmp;
  bmp.CreateCompatibleBitmap(&dc, clientW, clientH);
  CBitmap *pOldBmp = memDC.SelectObject(&bmp);

  // 背景填充（letterbox 区域）
  memDC.FillSolidRect(&rc, RGB(0, 0, 0));

  if (m_canvas) {
    // 按窗口大小等比缩放并居中绘制画布，画布保持原始全屏分辨率用于合成/保存
    int canvasW = m_canvas.GetWidth();
    int canvasH = m_canvas.GetHeight();
    if (canvasW > 0 && canvasH > 0 && clientW > 0 && clientH > 0) {
      double scale = (std::min)((double)clientW / (double)canvasW,
                                (double)clientH / (double)canvasH);
      int destW = (std::max)(1, (int)std::lround(canvasW * scale));
      int destH = (std::max)(1, (int)std::lround(canvasH * scale));
      int destLeft = (clientW - destW) / 2;
      int destTop = (clientH - destH) / 2;

      // 在内存 DC 上绘制缩放后的画布 — 使用 GDI+ 最近邻采样以避免模糊
      bool drewWithGdiPlus = false;
      HDC hCanvasDC = nullptr;
      HBITMAP hTmpBmp = nullptr;
      HDC hTempDC = nullptr;
      HBITMAP hOldTmp = nullptr;
      do {
        // 获取 m_canvas 的 HDC
        hCanvasDC = m_canvas.GetDC();
        if (!hCanvasDC)
          break;

        // 创建一个与画布同尺寸的兼容位图并把画布内容复制到该位图
        hTempDC = CreateCompatibleDC(hCanvasDC);
        if (!hTempDC)
          break;
        hTmpBmp = CreateCompatibleBitmap(hCanvasDC, canvasW, canvasH);
        if (!hTmpBmp)
          break;
        hOldTmp = (HBITMAP)SelectObject(hTempDC, hTmpBmp);
        if (!BitBlt(hTempDC, 0, 0, canvasW, canvasH, hCanvasDC, 0, 0, SRCCOPY))
          break;

        // 释放 m_canvas 的 DC（我们已经复制了像素）
        m_canvas.ReleaseDC();
        hCanvasDC = nullptr;

        // 从 HBITMAP 构造 GDI+ Bitmap，然后使用最近邻采样绘制到 memDC
        Bitmap *pGdiBitmap = Bitmap::FromHBITMAP(hTmpBmp, nullptr);
        if (!pGdiBitmap)
          break;
        {
          Graphics g(memDC.GetSafeHdc());
          g.SetInterpolationMode(InterpolationModeNearestNeighbor);
          Rect destRect(destLeft, destTop, destW, destH);
          g.DrawImage(pGdiBitmap, destRect, 0, 0, canvasW, canvasH, UnitPixel);
        }
        delete pGdiBitmap;
        drewWithGdiPlus = true;
      } while (false);

      // 清理临时 GDI 对象
      if (hTempDC) {
        if (hOldTmp)
          SelectObject(hTempDC, hOldTmp);
        DeleteDC(hTempDC);
      }
      if (hTmpBmp)
        DeleteObject(hTmpBmp);
      if (hCanvasDC)
        m_canvas.ReleaseDC();

      // 如果 GDI+ 流程失败，回退到原来的 Draw 调用
      if (!drewWithGdiPlus) {
        m_canvas.Draw(memDC.GetSafeHdc(), destLeft, destTop, destW, destH, 0, 0,
                      canvasW, canvasH);
      }
    }
  }

  // 一次性拷贝到窗口，减少闪烁
  dc.BitBlt(0, 0, clientW, clientH, &memDC, 0, 0, SRCCOPY);

  // 恢复并清理
  memDC.SelectObject(pOldBmp);
  bmp.DeleteObject();
  memDC.DeleteDC();
}

void CMonitorWnd::MonitorThreadFunc() {
  if (!m_pSocket)
    return;

  bool firstRequest = true;
  while (m_bIsMonitoring) {
    // 发送截屏请求。首次请求带标志位要求服务端强制发送全屏帧（避免服务端只回传 diff）。
    Cpacket req = (firstRequest ? Cpacket(CMD_SCREEN_CAPTURE, std::vector<BYTE>{1}) : Cpacket(CMD_SCREEN_CAPTURE, {}));
    firstRequest = false;
    if (!m_pSocket->SendPacket(req)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      continue;
    }

    std::optional<Cpacket> resp = m_pSocket->GetNextPacketBlocking(2000);
    if (!resp || resp->sCmd != CMD_SCREEN_CAPTURE) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    const std::vector<BYTE> &data = resp->data;
    if (data.size() < 16) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    int x = 0, y = 0, w = 0, h = 0;
    memcpy(&x, data.data(), 4);
    memcpy(&y, data.data() + 4, 4);
    memcpy(&w, data.data() + 8, 4);
    memcpy(&h, data.data() + 12, 4);

    size_t pngSize = data.size() - 16;
    const BYTE *pngData = data.data() + 16;

    if (pngSize == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    // 将 PNG 数据加载到 CImage
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, pngSize);
    if (!hMem) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    void *pMem = GlobalLock(hMem);
    memcpy(pMem, pngData, pngSize);
    GlobalUnlock(hMem);

    IStream *pStream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(hMem, TRUE, &pStream))) {
      GlobalFree(hMem);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    CImage diffImg;
    HRESULT hr = diffImg.Load(pStream);
    pStream->Release();
    if (FAILED(hr)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    // 在收到服务端帧后，若远端分辨率与本地记录不一致，按远端分辨率创建画布（首帧/分辨率变更）
    // 仅当该数据包为“全屏”包（x==0 && y==0）时，才用包头的 w/h 来创建画布。
    // 这避免了在重启/重新打开监视窗口时误将差异包的小尺寸当作远端全屏尺寸来初始化画布，
    // 导致只能显示部分屏幕内容的问题。
    {
      std::lock_guard<std::mutex> lock(m_canvasMutex);
      // 仅在尚未初始化画布（首次接收）时创建画布。差异包使用局部 w/h
      //（diffWidth/diffHeight），不应触发画布重建以保持合成效果。
      if (w > 0 && h > 0 && x == 0 && y == 0 && (m_remoteCanvasW == 0 || m_remoteCanvasH == 0)) {
        if (m_canvas)
          m_canvas.Destroy();
        // 创建与远端一致的画布用于合成和映射基准（首次帧应为全屏）
        m_canvas.Create(w, h, 24);
        m_remoteCanvasW = w;
        m_remoteCanvasH = h;
      }

      // 合成到画布（仅当画布已初始化时）
      if (m_remoteCanvasW > 0 && m_remoteCanvasH > 0) {
        HDC hCanvasDC = m_canvas.GetDC();
        if (hCanvasDC) {
          diffImg.Draw(hCanvasDC, x, y, w, h, 0, 0, w, h);
          m_canvas.ReleaseDC();
        }
      } else {
        // 如果画布尚未初始化且收到的是 diff 包（非全屏），跳过合成以避免对无效目标执行 Draw。
        // 日志仅为调试用途
        std::cout << "MonitorWnd: received diff before canvas initialization, skipping compose (x=" << x << ", y=" << y << ")" << std::endl;
      }
    }

    // 如果正在录制，保存整帧
    if (m_bIsRecording) {
      SaveFrameToDisk();
    }

    // 刷新窗口显示
    Invalidate(FALSE);

    // 控制帧率
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

void CMonitorWnd::SaveFrameToDisk() {
  try {
    std::filesystem::create_directories("recordings");
  } catch (...) {
    return;
  }

  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif

  std::ostringstream ss;
  ss << "recordings\\frame_" << std::put_time(&tm, "%Y%m%d_%H%M%S_")
     << m_frameIndex++ << ".png";
  std::string path = ss.str();

  // 保存时加锁，防止画布被写入
  std::lock_guard<std::mutex> lock(m_canvasMutex);
  if (m_canvas) {
    CString cpath(path.c_str());
    m_canvas.Save(cpath);
  }
}
