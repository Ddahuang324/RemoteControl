// MonitorWnd.h - 屏幕监视窗口（无资源对话框实现，基于 CWnd）
#pragma once

#include "pch.h"
#include "clientSocket.h"
#include <afxwin.h>
#include <atlimage.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>

class CMonitorWnd : public CWnd {
public:
    CMonitorWnd();
    virtual ~CMonitorWnd();

    // 创建窗口并启动监视线程
    bool CreateMonitorWindow(CWnd* pParent, CclientSocket* pSocket);
    // 停止监视线程并销毁窗口
    void StopMonitor();

    // 切换录制状态
    void ToggleRecording() { m_bIsRecording = !m_bIsRecording; }

protected:
    afx_msg void OnPaint();
    afx_msg void OnDestroy();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()

private:
    void MonitorThreadFunc();
    void SaveFrameToDisk();

    CclientSocket* m_pSocket = nullptr;
    CImage m_canvas;
    std::mutex m_canvasMutex;
    std::atomic<bool> m_bIsMonitoring{ false };
    std::atomic<bool> m_bIsRecording{ false };
    std::thread m_thread;
    int m_frameIndex = 0;
};
