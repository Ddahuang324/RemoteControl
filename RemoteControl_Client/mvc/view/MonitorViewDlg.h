#pragma once

#include "../interfaces/IController.h"
#include "../model/Interface.h"
#include "../resources/resource_mvc.h"
#include "afxdialogex.h"
#include "afxwin.h"
#include <atomic>
#include <memory>
#include <string>


// ============================================================================
// MonitorViewDlg: 监视窗口View
// 职责:
//   - 显示远程屏幕画面
//   - 处理鼠标键盘输入并转发给Controller
//   - 提供缩放、截图、录制等控制功能
// ============================================================================
class MonitorViewDlg : public CDialogEx {
public:
  explicit MonitorViewDlg(std::shared_ptr<IMonitorModel> monitor,
                          std::shared_ptr<IIoModel> io,
                          CWnd *pParent = nullptr);

  virtual ~MonitorViewDlg();

#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_MVC_MONITOR_DIALOG };
#endif

  // ---- View接口: 供Controller调用 ----

  // 更新显示帧
  void UpdateFrame(std::shared_ptr<const ::FrameData> frame);

  // 设置缩放比例
  void SetZoomScale(float scale);

  // 获取当前缩放比例
  float GetZoomScale() const { return m_fZoomScale; }

  // 更新状态栏
  void UpdateStatusBar(const std::string &text, int pane = 0);

  // 更新FPS显示
  void UpdateFPS(int fps);

  // 更新延迟显示
  void UpdateLatency(int ms);

  // Controller注入
  void SetController(std::shared_ptr<IMonitorController> controller);

protected:
  virtual void DoDataExchange(CDataExchange *pDX) override;
  virtual BOOL OnInitDialog() override;

  DECLARE_MESSAGE_MAP()

  // ---- 消息处理 ----

  afx_msg void OnPaint();
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg BOOL OnEraseBkgnd(CDC *pDC);

  // 鼠标事件
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);
  afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

  // 键盘事件
  afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);

  // 工具栏按钮
  afx_msg void OnToolbarMouseCtrl();
  afx_msg void OnToolbarKeyboardCtrl();
  afx_msg void OnToolbarScreenshot();
  afx_msg void OnToolbarRecord();
  afx_msg void OnToolbarZoomIn();
  afx_msg void OnToolbarZoomOut();
  afx_msg void OnToolbarZoomFit();
  afx_msg void OnToolbarZoomActual();
  afx_msg void OnToolbarFullscreen();

  // 快捷键
  virtual BOOL PreTranslateMessage(MSG *pMsg) override;

  // 窗口关闭
  afx_msg void OnClose();

private:
  // ---- Model接口 ----
  std::shared_ptr<IMonitorModel> monitor_;
  std::shared_ptr<IIoModel> io_;

  // ---- Controller ----
  std::shared_ptr<IMonitorController> controller_;

  // ---- UI控件 ----
  CToolBar m_toolbar;
  CStatusBar m_statusBar;
  CWnd* m_pCanvasWnd = nullptr; // 画布区域窗口指针（不使用DDX_Control）

  // ---- 显示状态 ----
  std::shared_ptr<const ::FrameData> m_currentFrame;
  float m_fZoomScale; // 缩放比例(1.0 = 100%)
  CRect m_canvasRect; // 画布区域矩形
  bool m_bFullscreen;

  // ---- 控制状态 ----
  bool m_bMouseControlEnabled;
  bool m_bKeyboardControlEnabled;
  bool m_bRecording;

  // ---- 性能指标 ----
  std::atomic<int> m_nCurrentFPS;
  std::atomic<int> m_nCurrentLatency;

  // ---- 辅助方法 ----

  // 初始化工具栏
  void InitToolbar();

  // 初始化状态栏
  void InitStatusBar();

  // 计算画布区域
  void CalculateCanvasRect();

  // 将窗口坐标转换为远程屏幕坐标
  CPoint WindowToRemote(CPoint pt);

  // 绘制帧到DC
  void DrawFrame(CDC *pDC);

  // 更新工具栏按钮状态
  void UpdateToolbarStates();
};
