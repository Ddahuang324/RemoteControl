#pragma once

#include "pch.h"
#include "clientSocket.h"
#include "Resource.h"
#include <atlimage.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

// 与 ScreenViewDlg 保持一致的更新消息
#define WM_UPDATE_SCREEN (WM_USER + 200)

class CMonitorWnd : public CDialog {
  DECLARE_DYNAMIC(CMonitorWnd)
public:
  CMonitorWnd(CWnd *pParent = nullptr);
  virtual ~CMonitorWnd();

  enum { IDD = IDD_MONITOR_DIALOG };

  // 创建与停止监控窗口
  bool CreateMonitorWindow(CWnd *pParent, CClientSocket *pSocket);
  void StopMonitor();

  // 切换录制
  void ToggleRecording() { m_bIsRecording = !m_bIsRecording; }

protected:
  virtual void DoDataExchange(CDataExchange *pDX);
  virtual BOOL OnInitDialog();

  afx_msg void OnPaint();
  afx_msg void OnDestroy();
  afx_msg BOOL OnEraseBkgnd(CDC *pDC);
  afx_msg LRESULT OnUpdateScreen(WPARAM wParam, LPARAM lParam);
  afx_msg void OnSize(UINT nType, int cx, int cy);

  // 按钮事件
  afx_msg void OnBnClickedBtnLock();
  afx_msg void OnBnClickedBtnUnlock();
  afx_msg void OnBnClickedCheckMouseControl();

  // 鼠标事件
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnMButtonUp(UINT nFlags, CPoint point);

  DECLARE_MESSAGE_MAP()

private:
  void MonitorThreadFunc();
  void SaveFrameToDisk();
  void StartReceiveLoop();
  void StopReceiveLoop();

  // 坐标映射与发送
  bool MapClientPointToRemote(const CPoint &clientPt, CPoint &outRemotePt);
  void SendMouseEventToServer(WORD nButton, WORD nAction, const CPoint &remotePt);

  CClientSocket *m_pSocket = nullptr;
  CImage m_canvas;
  std::mutex m_canvasMutex;

  int m_remoteCanvasW = 0;
  int m_remoteCanvasH = 0;
  std::atomic<bool> m_bIsMonitoring{false};
  std::atomic<bool> m_bIsRecording{false};
  std::thread m_thread;
  std::thread m_receiveThread;
  std::atomic<bool> m_receiving{false};
  int m_frameIndex = 0;

  // 鼠标控制相关
  std::atomic<bool> m_bMouseControlEnabled{false};
  std::chrono::steady_clock::time_point m_lastMoveTime = std::chrono::steady_clock::now();
  int m_moveThrottleMs = 30;
  // (no extra exit button here; control is in main dialog)
};
