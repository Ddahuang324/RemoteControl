#pragma once

#include "afxdialogex.h"
#include "clientSocket.h"
#include <atlimage.h>
#include <atomic>
#include <thread>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

// 自定义消息：用于从网络线程向 UI 线程传递屏幕图像数据
#define WM_UPDATE_SCREEN (WM_USER + 200)

class CScreenViewDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CScreenViewDlg)

public:
    CScreenViewDlg(CClientSocket* pSocket, CWnd* pParent = nullptr);
    virtual ~CScreenViewDlg();
    
    // GDI+初始化辅助函数
    static bool InitializeGDIPlus();
    static void ShutdownGDIPlus();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIALOG_SCREEN_VIEW };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    DECLARE_MESSAGE_MAP()

private:
    CClientSocket* m_pSocket; // 与服务器通信的 socket 指针
    CImage m_image;           // 用于加载并绘制 PNG 图像
    CStatic m_pictureCtrl;    // 用于显示图像的静态控件
    // 网络事件由 CClientSocket 通过 PostMessage 发送到本窗口（WM_UPDATE_SCREEN）
    std::thread m_receiveThread;
    std::atomic<bool> m_receiving{ false };

    void StartReceiveLoop();
    void StopReceiveLoop();

protected:
    afx_msg void OnPaint();
    afx_msg void OnClose();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg LRESULT OnUpdateScreen(WPARAM wParam, LPARAM lParam);
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
