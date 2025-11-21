// MonitorWnd.h - 屏幕监视窗口（无资源对话框实现，基于 CWnd）
// encoding: UTF-8
#pragma once

#include "clientSocket.h"
#include "pch.h"
#include <afxwin.h>
#include <atlimage.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>


#include "Resource.h"

class CMonitorWnd : public CDialog {
  DECLARE_DYNAMIC(CMonitorWnd)
public:
  CMonitorWnd(CWnd *pParent = nullptr);
  virtual ~CMonitorWnd();

  enum { IDD = IDD_MONITOR_DIALOG };

  // 创建窗口并启动监视线程
  bool CreateMonitorWindow(CWnd *pParent, CclientSocket *pSocket);
  // 停止监视线程并销毁窗口
  void StopMonitor();

  // 切换录制状态
  void ToggleRecording() { m_bIsRecording = !m_bIsRecording; }

protected:
  virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV 支持
  virtual BOOL OnInitDialog();

  afx_msg void OnPaint();
  afx_msg void OnDestroy();
  afx_msg BOOL OnEraseBkgnd(CDC *pDC);
  afx_msg void OnSize(UINT nType, int cx, int cy);

  // 按钮事件
  afx_msg void OnBnClickedBtnLock();
  afx_msg void OnBnClickedBtnUnlock();
  afx_msg void OnBnClickedCheckMouseControl();

  DECLARE_MESSAGE_MAP()

  // 鼠标消息处理
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnMButtonUp(UINT nFlags, CPoint point);

private:
  void MonitorThreadFunc();
  void SaveFrameToDisk();

  // 把 client 窗口坐标映射到远端 canvas 像素坐标（若在 letterbox 外返回 false）
  bool MapClientPointToRemote(const CPoint &clientPt, CPoint &outRemotePt);

  // 封装并发送鼠标事件到服务端（使用 Enities::MouseEventData）
  void SendMouseEventToServer(WORD nButton, WORD nAction,
                              const CPoint &remotePt);

  CclientSocket *m_pSocket = nullptr;
  CImage m_canvas;
  std::mutex m_canvasMutex;
  // 远端画布尺寸（由服务端首帧或单独分辨率消息提供）
  int m_remoteCanvasW = 0;
  int m_remoteCanvasH = 0;
  std::atomic<bool> m_bIsMonitoring{false};
  std::atomic<bool> m_bIsRecording{false};
  std::thread m_thread;
  int m_frameIndex = 0;

  // 是否允许远程鼠标控制（由 UI 控制）
  std::atomic<bool> m_bMouseControlEnabled{false};

  // 鼠标移动节流（毫秒）与上次发送时间点
  std::chrono::steady_clock::time_point m_lastMoveTime =
      std::chrono::steady_clock::now();
  int m_moveThrottleMs = 30;
};
