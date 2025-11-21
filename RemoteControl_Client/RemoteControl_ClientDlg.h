
// RemoteControl_ClientDlg.h: 头文件
//

#pragma once
#include "clientSocket.h"
#include <string>
#include <vector>
#include <optional>
#include "Enities.h"
#include "include\MultiThread\ThreadPool.hpp"

// 前向声明
class CRemoteControlClientDlg;
class CDownloadProgressDlg;
class CMonitorWnd;

// 定义自定义消息
#define WM_UPDATE_PROGRESS (WM_USER + 1)
#define WM_CLOSE_PROGRESS (WM_USER + 2)
// 客户端断开连接通知（由后台线程发送到 UI 线程）
#define WM_CLIENT_DISCONNECTED (WM_USER + 3)
// 监视停止通知（由后台线程发送到 UI 线程）
#define WM_CLIENT_MONITOR_STOPPED (WM_USER + 4)
// 通知主线程销毁并删除监视窗口对象（LPARAM = CMonitorWnd*）
#define WM_CLIENT_MONITOR_DESTROY (WM_USER + 5)

// 下载参数结构体
struct DownloadParams {
    CRemoteControlClientDlg* pDlg;
    std::string filePath;
    std::string savePath;
    std::streamsize fileSize;
    CclientSocket* pSocket;
    CDownloadProgressDlg* pProgressDlg;
};

// CRemoteControlClientDlg 对话框

#include <afxcmn.h> // CTreeCtrl, CListCtrl, CIPAddressCtrl
#include <afxwin.h> // CEdit, CButton

class CRemoteControlClientDlg : public CDialogEx
{
// 构造
public:
	CRemoteControlClientDlg(CWnd* pParent = nullptr);    // 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_REMOTECONTROL_CLIENT_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV 支持

// 实现
protected:
	HICON m_hIcon;

// public:
	CclientSocket m_clientSocket;

		// 新 UI 控件声明（用于补全代码）
		CTreeCtrl m_Tree;              // 目录树 IDC_TREE3
		CListCtrl m_List;              // 文件列表 IDC_LIST4

		// IP和端口成员变量（用于补全代码）
		DWORD m_Serv_Address = 0x7F000001; // 127.0.0.1
		CString m_Port = _T("12345");

		// 兼容原有成员变量
		CEdit m_editPort;                 // 端口输入框 IDC_EDIT1
		CIPAddressCtrl m_ipAddressServ;   // IP 地址控件 IDC_IPADDRESS_Serv
		CButton m_btnViewFileInfo;        // 查看文件信息按钮 IDC_BUTTON2
		CButton m_btnConnect;             // 连接按钮 IDC_BTN_TEST

		// 屏幕监视按钮（由资源定义）
		CButton m_btnStartMonitor;

		// 活动的监视窗口指针（若为空表示未启动监视）
		CMonitorWnd* m_pMonitorWnd = nullptr;

		CString m_strServerIP = L"127.0.0.1"; // 当前服务器IP
		int m_nServerPort = 9527; // 当前服务器端口，默认9527

		CString m_strCurrentDirPath; // 当前目录路径
		CString m_strSelectedFile; // 选中的文件

		bool m_bConnected = false; // 连接状态

		ThreadPool m_threadPool; // 线程池

		// 生成的消息映射函数
		virtual BOOL OnInitDialog()override;
		afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
		afx_msg void OnPaint();
		afx_msg HCURSOR OnQueryDragIcon();
		afx_msg void OnBnClickedBtnTest();
		afx_msg void OnBnClickedButton2();
		afx_msg void OnTvnSelchangedTree3(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnDblclkTree3(NMHDR* pNMHDR, LRESULT* pResult); // 新增：树控件双击
		afx_msg CString GetItemPath(HTREEITEM hItem); // 新增：递归获取路径
		afx_msg void OnLvnItemchangedList4(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnIpnFieldchangedIpaddressServ(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnEnChangeEdit1();
		afx_msg void OnNMRClickList4(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnBnClickedBtnStartMonitor();
		// 启动屏幕监视窗口（可在资源中绑定到按钮）
		void StartMonitor();
		afx_msg void OnDownloadFile();
		afx_msg void OnDeleteFile();
		afx_msg void OnOpenFile();
		afx_msg LRESULT OnUpdateProgress(WPARAM wParam, LPARAM lParam);
		afx_msg LRESULT OnCloseProgress(WPARAM wParam, LPARAM lParam);
		// 后台断开连接完成后通知主线程更新 UI
		afx_msg LRESULT OnClientDisconnected(WPARAM wParam, LPARAM lParam);
		afx_msg LRESULT OnMonitorStopped(WPARAM wParam, LPARAM lParam);
		// 在 UI 线程收到请求后销毁监视窗口对象
		afx_msg LRESULT OnMonitorDestroyPosted(WPARAM wParam, LPARAM lParam);
		DECLARE_MESSAGE_MAP()
};
