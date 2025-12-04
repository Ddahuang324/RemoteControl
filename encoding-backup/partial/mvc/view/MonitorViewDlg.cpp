#include "pch.h"
#pragma execution_character_set("utf-8")
#include "MonitorViewDlg.h"
#include <algorithm>

// 文件编码: UTF-8

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ============================================================================
// 构造与析构
// ============================================================================

MonitorViewDlg::MonitorViewDlg(std::shared_ptr<IMonitorModel> monitor,
                               std::shared_ptr<IIoModel> io, CWnd *pParent)
    : CDialogEx(IDD_MVC_MONITOR_DIALOG, pParent), monitor_(monitor), io_(io),
      controller_(nullptr), m_fZoomScale(1.0f), m_bFullscreen(false),
      m_bMouseControlEnabled(false), m_bKeyboardControlEnabled(false),
      m_bRecording(false), m_nCurrentFPS(0), m_nCurrentLatency(0) {}

MonitorViewDlg::~MonitorViewDlg() {}

void MonitorViewDlg::DoDataExchange(CDataExchange *pDX) {
  CDialogEx::DoDataExchange(pDX);
  // 不使用 DDX_Control 绑定 m_canvas，改用 OnInitDialog 中 GetDlgItem
}

BEGIN_MESSAGE_MAP(MonitorViewDlg, CDialogEx)
ON_WM_PAINT()
ON_WM_SIZE()
ON_WM_ERASEBKGND()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_RBUTTONDOWN()
ON_WM_RBUTTONUP()
ON_WM_MBUTTONDOWN()
ON_WM_MBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_MOUSEWHEEL()
ON_WM_KEYDOWN()
ON_WM_KEYUP()
ON_WM_CLOSE()

// 工具栏
ON_COMMAND(ID_MVC_MONITOR_MOUSE_CTRL, &MonitorViewDlg::OnToolbarMouseCtrl)
ON_COMMAND(ID_MVC_MONITOR_KEYBOARD_CTRL, &MonitorViewDlg::OnToolbarKeyboardCtrl)
ON_COMMAND(ID_MVC_MONITOR_SCREENSHOT, &MonitorViewDlg::OnToolbarScreenshot)
ON_COMMAND(ID_MVC_MONITOR_RECORD, &MonitorViewDlg::OnToolbarRecord)
ON_COMMAND(ID_MVC_MONITOR_ZOOM_IN, &MonitorViewDlg::OnToolbarZoomIn)
ON_COMMAND(ID_MVC_MONITOR_ZOOM_OUT, &MonitorViewDlg::OnToolbarZoomOut)
ON_COMMAND(ID_MVC_MONITOR_ZOOM_FIT, &MonitorViewDlg::OnToolbarZoomFit)
ON_COMMAND(ID_MVC_MONITOR_ZOOM_ACTUAL, &MonitorViewDlg::OnToolbarZoomActual)
ON_COMMAND(ID_MVC_MONITOR_FULLSCREEN, &MonitorViewDlg::OnToolbarFullscreen)
END_MESSAGE_MAP()

// ============================================================================
// 初始化
// ============================================================================

BOOL MonitorViewDlg::OnInitDialog() {
  CDialogEx::OnInitDialog();

  // 手动获取画布控件指针（不使用 DDX_Control 避免 SubclassDlgItem 断言）
  m_pCanvasWnd = GetDlgItem(IDC_MVC_MONITOR_CANVAS);

  // 初始化工具栏
  InitToolbar();

  // 初始化状态栏
  InitStatusBar();

  // 计算画布区域
  CalculateCanvasRect();

  return TRUE;
}

void MonitorViewDlg::InitToolbar() {
  // 直接创建工具栏，不使用 SubclassDlgItem（CToolBar 不支持子类化普通控件）
  if (!m_toolbar.CreateEx(this, TBSTYLE_FLAT,
                          WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS |
                              CBRS_FLYBY)) {
    TRACE0("Failed to create monitor toolbar\n");
    return;
  }

  TBBUTTON buttons[] = {
      {0,
       ID_MVC_MONITOR_MOUSE_CTRL,
       TBSTATE_ENABLED,
       TBSTYLE_CHECK,
       {0},
       0,
       (INT_PTR) _T("鼠标控制")},
      {1,
       ID_MVC_MONITOR_KEYBOARD_CTRL,
       TBSTATE_ENABLED,
       TBSTYLE_CHECK,
       {0},
       0,
       (INT_PTR) _T("键盘控制")},
      {0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0},
      {2,
       ID_MVC_MONITOR_SCREENSHOT,
       TBSTATE_ENABLED,
       TBSTYLE_BUTTON,
       {0},
       0,
       (INT_PTR) _T("截图")},
      {3,
       ID_MVC_MONITOR_RECORD,
       TBSTATE_ENABLED,
       TBSTYLE_CHECK,
       {0},
       0,
       (INT_PTR) _T("录制")},
      {0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0},
      {4,
       ID_MVC_MONITOR_ZOOM_IN,
       TBSTATE_ENABLED,
       TBSTYLE_BUTTON,
       {0},
       0,
       (INT_PTR) _T("放大")},
      {5,
       ID_MVC_MONITOR_ZOOM_OUT,
       TBSTATE_ENABLED,
       TBSTYLE_BUTTON,
       {0},
       0,
       (INT_PTR) _T("缩小")},
      {6,
       ID_MVC_MONITOR_ZOOM_FIT,
       TBSTATE_ENABLED,
       TBSTYLE_BUTTON,
       {0},
       0,
       (INT_PTR) _T("适应窗口")},
      {7,
       ID_MVC_MONITOR_ZOOM_ACTUAL,
       TBSTATE_ENABLED,
       TBSTYLE_BUTTON,
       {0},
       0,
       (INT_PTR) _T("实际大小")},
      {0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0},
      {8,
       ID_MVC_MONITOR_FULLSCREEN,
       TBSTATE_ENABLED,
       TBSTYLE_BUTTON,
       {0},
       0,
       (INT_PTR) _T("全屏")},
  };

  m_toolbar.GetToolBarCtrl().AddButtons(_countof(buttons), buttons);
  RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
}

void MonitorViewDlg::InitStatusBar() {
  // 直接创建状态栏，不使用 SubclassDlgItem（CStatusBar 不支持子类化普通控件）
  if (!m_statusBar.Create(this)) {
    TRACE0("Failed to create monitor statusbar\n");
    return;
  }

  static UINT indicators[] = {
      ID_SEPARATOR, // 状态文本
      ID_SEPARATOR, // FPS
      ID_SEPARATOR, // 延迟
      ID_SEPARATOR, // 分辨率
      ID_SEPARATOR, // 缩放比例
  };

  m_statusBar.SetIndicators(indicators, _countof(indicators));
  m_statusBar.SetPaneInfo(0, ID_SEPARATOR, SBPS_STRETCH, 0);
  m_statusBar.SetPaneInfo(1, ID_SEPARATOR, SBPS_NORMAL, 80);
  m_statusBar.SetPaneInfo(2, ID_SEPARATOR, SBPS_NORMAL, 100);
  m_statusBar.SetPaneInfo(3, ID_SEPARATOR, SBPS_NORMAL, 120);
  m_statusBar.SetPaneInfo(4, ID_SEPARATOR, SBPS_NORMAL, 80);

  UpdateStatusBar("就绪", 0);
  UpdateFPS(0);
  UpdateLatency(0);
  m_statusBar.SetPaneText(3, _T("分辨率: --"));

  CString zoomText;
  zoomText.Format(_T("%.0f%%"), m_fZoomScale * 100);
  m_statusBar.SetPaneText(4, zoomText);

  RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
}

void MonitorViewDlg::CalculateCanvasRect() {
  CRect clientRect;
  GetClientRect(&clientRect);

  // 减去工具栏和状态栏的高度
  CRect toolbarRect, statusRect;
  if (m_toolbar.GetSafeHwnd()) {
    m_toolbar.GetWindowRect(&toolbarRect);
    clientRect.top += toolbarRect.Height();
  }
  if (m_statusBar.GetSafeHwnd()) {
    m_statusBar.GetWindowRect(&statusRect);
    clientRect.bottom -= statusRect.Height();
  }

  m_canvasRect = clientRect;
}

// ============================================================================
// Controller注入
// ============================================================================

void MonitorViewDlg::SetController(
    std::shared_ptr<IMonitorController> controller) {
  controller_ = controller;
}

// ============================================================================
// View接口实现
// ============================================================================

void MonitorViewDlg::UpdateFrame(std::shared_ptr<const ::FrameData> frame) {
  m_currentFrame = frame;

  // 更新分辨率显示
  if (frame && m_statusBar.GetSafeHwnd()) {
    CString resText;
    resText.Format(_T("分辨率: %dx%d"), frame->width, frame->height);
    m_statusBar.SetPaneText(3, resText);
  }

  // 触发重绘
  if (m_pCanvasWnd && m_pCanvasWnd->GetSafeHwnd()) {
    m_pCanvasWnd->Invalidate(FALSE);
  } else {
    Invalidate(FALSE);
  }
}

void MonitorViewDlg::SetZoomScale(float scale) {
  m_fZoomScale = std::max(0.1f, std::min(5.0f, scale));

  // 更新状态栏
  CString zoomText;
  zoomText.Format(_T("%.0f%%"), m_fZoomScale * 100);
  m_statusBar.SetPaneText(4, zoomText);

  // 重新计算画布并重绘
  CalculateCanvasRect();
  Invalidate();
}

void MonitorViewDlg::UpdateStatusBar(const std::string &text, int pane) {
  if (m_statusBar.GetSafeHwnd()) {
    m_statusBar.SetPaneText(pane, CString(text.c_str()));
  }
}

void MonitorViewDlg::UpdateFPS(int fps) {
  m_nCurrentFPS = fps;
  if (m_statusBar.GetSafeHwnd()) {
    CString text;
    text.Format(_T("FPS: %d"), fps);
    m_statusBar.SetPaneText(1, text);
  }
}

void MonitorViewDlg::UpdateLatency(int ms) {
  m_nCurrentLatency = ms;
  if (m_statusBar.GetSafeHwnd()) {
    CString text;
    text.Format(_T("延迟: %d ms"), ms);
    m_statusBar.SetPaneText(2, text);
  }
}

// ============================================================================
// 消息处理
// ============================================================================

void MonitorViewDlg::OnPaint() {
  if (IsIconic()) {
    CPaintDC dc(this);
    // 最小化时的绘制
  } else {
    CPaintDC dc(this);
    DrawFrame(&dc);
  }
}

void MonitorViewDlg::DrawFrame(CDC *pDC) {
  if (!m_currentFrame || (m_currentFrame->rgba.empty() &&
                          m_currentFrame->compressedPayload.empty())) {
    // 没有帧数据,绘制黑色背景
    CBrush brush(RGB(0, 0, 0));
    pDC->FillRect(&m_canvasRect, &brush);

    // 绘制提示文字
    pDC->SetTextColor(RGB(255, 255, 255));
    pDC->SetBkMode(TRANSPARENT);
    pDC->DrawText(_T("等待远程屏幕数据..."), &m_canvasRect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return;
  }

  // TODO: 实际绘制需要根据FrameData的格式(JPEG/PNG/RAW)进行解码
  // 这里提供一个简化的示例框架

  // 计算缩放后的尺寸
  int scaledWidth = (int)(m_currentFrame->width * m_fZoomScale);
  int scaledHeight = (int)(m_currentFrame->height * m_fZoomScale);

  // 居中显示
  int x = m_canvasRect.left + (m_canvasRect.Width() - scaledWidth) / 2;
  int y = m_canvasRect.top + (m_canvasRect.Height() - scaledHeight) / 2;

  // 绘制边框
  CRect frameRect(x, y, x + scaledWidth, y + scaledHeight);
  pDC->Draw3dRect(&frameRect, RGB(100, 100, 100), RGB(50, 50, 50));

  // TODO: 解码并绘制实际图像
  // 示例: 使用GDI+加载JPEG并绘制
  // 或使用StretchDIBits绘制原始位图数据
}

BOOL MonitorViewDlg::OnEraseBkgnd(CDC *pDC) {
  // 防止闪烁,在OnPaint中统一绘制
  return TRUE;
}

void MonitorViewDlg::OnSize(UINT nType, int cx, int cy) {
  CDialogEx::OnSize(nType, cx, cy);

  CalculateCanvasRect();

  if (m_toolbar.GetSafeHwnd() && m_statusBar.GetSafeHwnd()) {
    RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
  }
}

// ============================================================================
// 鼠标事件
// ============================================================================

CPoint MonitorViewDlg::WindowToRemote(CPoint pt) {
  if (!m_currentFrame)
    return CPoint(0, 0);

  // 计算画布中心位置
  int scaledWidth = (int)(m_currentFrame->width * m_fZoomScale);
  int scaledHeight = (int)(m_currentFrame->height * m_fZoomScale);
  int x = m_canvasRect.left + (m_canvasRect.Width() - scaledWidth) / 2;
  int y = m_canvasRect.top + (m_canvasRect.Height() - scaledHeight) / 2;

  // 转换为远程坐标
  int remoteX = (int)((pt.x - x) / m_fZoomScale);
  int remoteY = (int)((pt.y - y) / m_fZoomScale);

  // 限制在有效范围内
  remoteX = std::max(0, std::min(remoteX, (int)m_currentFrame->width - 1));
  remoteY = std::max(0, std::min(remoteY, (int)m_currentFrame->height - 1));

  return CPoint(remoteX, remoteY);
}

void MonitorViewDlg::OnLButtonDown(UINT nFlags, CPoint point) {
  if (controller_ && m_bMouseControlEnabled) {
    CPoint remote = WindowToRemote(point);
    controller_->OnMouseInput(remote.x, remote.y, 0, true);
  }
  CDialogEx::OnLButtonDown(nFlags, point);
}

void MonitorViewDlg::OnLButtonUp(UINT nFlags, CPoint point) {
  if (controller_ && m_bMouseControlEnabled) {
    CPoint remote = WindowToRemote(point);
    controller_->OnMouseInput(remote.x, remote.y, 0, false);
  }
  CDialogEx::OnLButtonUp(nFlags, point);
}

void MonitorViewDlg::OnRButtonDown(UINT nFlags, CPoint point) {
  if (controller_ && m_bMouseControlEnabled) {
    CPoint remote = WindowToRemote(point);
    controller_->OnMouseInput(remote.x, remote.y, 1, true);
  }
  CDialogEx::OnRButtonDown(nFlags, point);
}

void MonitorViewDlg::OnRButtonUp(UINT nFlags, CPoint point) {
  if (controller_ && m_bMouseControlEnabled) {
    CPoint remote = WindowToRemote(point);
    controller_->OnMouseInput(remote.x, remote.y, 1, false);
  }
  CDialogEx::OnRButtonUp(nFlags, point);
}

void MonitorViewDlg::OnMButtonDown(UINT nFlags, CPoint point) {
  if (controller_ && m_bMouseControlEnabled) {
    CPoint remote = WindowToRemote(point);
    controller_->OnMouseInput(remote.x, remote.y, 2, true);
  }
  CDialogEx::OnMButtonDown(nFlags, point);
}

void MonitorViewDlg::OnMButtonUp(UINT nFlags, CPoint point) {
  if (controller_ && m_bMouseControlEnabled) {
    CPoint remote = WindowToRemote(point);
    controller_->OnMouseInput(remote.x, remote.y, 2, false);
  }
  CDialogEx::OnMButtonUp(nFlags, point);
}

void MonitorViewDlg::OnMouseMove(UINT nFlags, CPoint point) {
  if (controller_ && m_bMouseControlEnabled) {
    CPoint remote = WindowToRemote(point);
    controller_->OnMouseMove(remote.x, remote.y);
  }
  CDialogEx::OnMouseMove(nFlags, point);
}

BOOL MonitorViewDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {
  // 使用Ctrl+滚轮进行缩放
  if (nFlags & MK_CONTROL) {
    float newScale = m_fZoomScale + (zDelta > 0 ? 0.1f : -0.1f);
    SetZoomScale(newScale);
    return TRUE;
  }

  return CDialogEx::OnMouseWheel(nFlags, zDelta, pt);
}

// ============================================================================
// 键盘事件
// ============================================================================

void MonitorViewDlg::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
  if (controller_ && m_bKeyboardControlEnabled) {
    controller_->OnKeyInput(nChar, true);
  }
  CDialogEx::OnKeyDown(nChar, nRepCnt, nFlags);
}

void MonitorViewDlg::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) {
  if (controller_ && m_bKeyboardControlEnabled) {
    controller_->OnKeyInput(nChar, false);
  }
  CDialogEx::OnKeyUp(nChar, nRepCnt, nFlags);
}

BOOL MonitorViewDlg::PreTranslateMessage(MSG *pMsg) {
  // 处理快捷键
  if (pMsg->message == WM_KEYDOWN) {
    // Ctrl+Alt+F: 全屏
    if (GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_MENU) < 0 &&
        pMsg->wParam == 'F') {
      OnToolbarFullscreen();
      return TRUE;
    }
    // Ctrl+Alt+S: 截图
    if (GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_MENU) < 0 &&
        pMsg->wParam == 'S') {
      OnToolbarScreenshot();
      return TRUE;
    }
    // Ctrl+Alt+R: 录制
    if (GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_MENU) < 0 &&
        pMsg->wParam == 'R') {
      OnToolbarRecord();
      return TRUE;
    }
    // ESC: 退出全屏
    if (m_bFullscreen && pMsg->wParam == VK_ESCAPE) {
      OnToolbarFullscreen();
      return TRUE;
    }
  }

  return CDialogEx::PreTranslateMessage(pMsg);
}

// ============================================================================
// 工具栏按钮
// ============================================================================

void MonitorViewDlg::OnToolbarMouseCtrl() {
  m_bMouseControlEnabled = !m_bMouseControlEnabled;

  if (controller_) {
    controller_->OnMouseControlToggle(m_bMouseControlEnabled);
  }

  // 更新按钮状态
  m_toolbar.GetToolBarCtrl().CheckButton(ID_MVC_MONITOR_MOUSE_CTRL,
                                         m_bMouseControlEnabled);

  UpdateStatusBar(m_bMouseControlEnabled ? "鼠标控制已启用" : "鼠标控制已禁用",
                  0);
}

void MonitorViewDlg::OnToolbarKeyboardCtrl() {
  m_bKeyboardControlEnabled = !m_bKeyboardControlEnabled;

  if (controller_) {
    controller_->OnKeyboardControlToggle(m_bKeyboardControlEnabled);
  }

  m_toolbar.GetToolBarCtrl().CheckButton(ID_MVC_MONITOR_KEYBOARD_CTRL,
                                         m_bKeyboardControlEnabled);

  UpdateStatusBar(
      m_bKeyboardControlEnabled ? "键盘控制已启用" : "键盘控制已禁用", 0);
}

void MonitorViewDlg::OnToolbarScreenshot() {
  if (!controller_)
    return;

  // 弹出保存对话框
  CFileDialog dlg(FALSE, _T("png"), _T("screenshot"),
                  OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                  _T("PNG图片 (*.png)|*.png|JPEG图片 (*.jpg)|*.jpg||"), this);

  if (dlg.DoModal() == IDOK) {
    CString path = dlg.GetPathName();
    controller_->OnSaveScreenshot(std::string(CT2A(path)));
    UpdateStatusBar("截图已保存", 0);
  }
}

void MonitorViewDlg::OnToolbarRecord() {
  m_bRecording = !m_bRecording;

  if (controller_) {
    if (m_bRecording) {
      controller_->OnStartRecording(""); // 使用默认目录
      UpdateStatusBar("录制中...", 0);
    } else {
      controller_->OnStopRecording();
      UpdateStatusBar("录制已停止", 0);
    }
  }

  m_toolbar.GetToolBarCtrl().CheckButton(ID_MVC_MONITOR_RECORD, m_bRecording);
}

void MonitorViewDlg::OnToolbarZoomIn() { SetZoomScale(m_fZoomScale + 0.25f); }

void MonitorViewDlg::OnToolbarZoomOut() { SetZoomScale(m_fZoomScale - 0.25f); }

void MonitorViewDlg::OnToolbarZoomFit() {
  if (controller_) {
    controller_->OnFitToWindow();
  }

  // 计算适应窗口的缩放比例
  if (m_currentFrame) {
    float scaleX = (float)m_canvasRect.Width() / m_currentFrame->width;
    float scaleY = (float)m_canvasRect.Height() / m_currentFrame->height;
    SetZoomScale(std::min(scaleX, scaleY));
  }
}

void MonitorViewDlg::OnToolbarZoomActual() {
  if (controller_) {
    controller_->OnActualSize();
  }
  SetZoomScale(1.0f);
}

void MonitorViewDlg::OnToolbarFullscreen() {
  m_bFullscreen = !m_bFullscreen;

  if (m_bFullscreen) {
    // 进入全屏
    ModifyStyle(WS_CAPTION | WS_THICKFRAME, 0);
    m_toolbar.ShowWindow(SW_HIDE);
    m_statusBar.ShowWindow(SW_HIDE);

    CRect screenRect;
    ::GetWindowRect(::GetDesktopWindow(), &screenRect);
    SetWindowPos(NULL, 0, 0, screenRect.Width(), screenRect.Height(),
                 SWP_NOZORDER | SWP_FRAMECHANGED);
  } else {
    // 退出全屏
    ModifyStyle(0, WS_CAPTION | WS_THICKFRAME);
    m_toolbar.ShowWindow(SW_SHOW);
    m_statusBar.ShowWindow(SW_SHOW);

    SetWindowPos(NULL, 0, 0, 900, 620,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
  }

  CalculateCanvasRect();
  Invalidate();
}

void MonitorViewDlg::OnClose() {
  // 停止捕获
  if (controller_) {
    controller_->OnStopCapture();

    if (m_bRecording) {
      controller_->OnStopRecording();
    }
  }

  CDialogEx::OnClose();
}

void MonitorViewDlg::UpdateToolbarStates() {
  if (!m_toolbar.GetSafeHwnd())
    return;

  m_toolbar.GetToolBarCtrl().CheckButton(ID_MVC_MONITOR_MOUSE_CTRL,
                                         m_bMouseControlEnabled);
  m_toolbar.GetToolBarCtrl().CheckButton(ID_MVC_MONITOR_KEYBOARD_CTRL,
                                         m_bKeyboardControlEnabled);
  m_toolbar.GetToolBarCtrl().CheckButton(ID_MVC_MONITOR_RECORD, m_bRecording);
}
