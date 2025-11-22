
// RemoteControl_ClientDlg.h: 头文件
//

#pragma once
#include "clientSocket.h"
#include "Enities.h"
#include "include\\MultiThread\\ThreadPool.hpp"
#include "ScreenViewDlg.h"

// 前向声明
class CDownloadProgressDlg;
class CMonitorWnd;

// 定义自定义消息
#define WM_UPDATE_PROGRESS (WM_USER + 1)
#define WM_CLOSE_PROGRESS (WM_USER + 2)

// 下载参数结构体
struct DownloadParams {
	CString requestPath;
	CString savePath;
	std::streamsize fileSize;
	// 使用指向 CClientSocket 的方法通过单例访问
	CDownloadProgressDlg* pProgressDlg;
	class CRemoteControlClientDlg* pOwner;
};

// CRemoteControlClientDlg 对话框
class CRemoteControlClientDlg : public CDialogEx
{
// 构造
public:
	CRemoteControlClientDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_REMOTECONTROL_CLIENT_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;	// DDX/DDV 支持


// 实现
private:
	bool SendCommandPacket(int nCommand, const BYTE* pData = NULL, size_t nSize = 0);



protected:
	HICON m_hIcon;

// 网络客户端
	CClientSocket& m_client = CClientSocket::GetInstance();

	// UI 控件
	CButton m_btnConnect; // IDC_BTN_TEST
	CButton m_btnWatchScreen; // IDC_BTN_WATCH_SCREEN
	CMonitorWnd* m_pMonitorWnd = nullptr;

	// monitoring state
	bool m_bIsMonitoring = false;

	// 状态
	CString m_strCurrentDirPath;
	CString m_strSelectedFile;
	bool m_bConnected = false;

	// 线程池
	ThreadPool m_threadPool{4};

	// 生成的消息映射函数
	virtual BOOL OnInitDialog() override;
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedBtnTest();
	afx_msg void OnBnClickedBtnWatchScreen();
	afx_msg void OnBnClickedButton2();
	DWORD m_Serv_Address;
	CString m_Port;
	afx_msg void OnTvnSelchangedTree3(NMHDR* pNMHDR, LRESULT* pResult);
	CTreeCtrl m_Tree;
	CString GetSelectedDrive() const;
	CString GetItemPath(HTREEITEM hItem);
	afx_msg void OnDblclkTree3(NMHDR* pNMHDR, LRESULT* pResult);
	CListCtrl m_List;
	afx_msg void OnNMRClickList4(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDownloadFile();
	afx_msg void OnDeleteFile();
	afx_msg void OnOpenFile();
	afx_msg LRESULT OnUpdateProgress(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnCloseProgress(WPARAM wParam, LPARAM lParam);
};

typedef struct file_info {
    bool isValid;
    bool isDir;
    char szFileName[256];
    bool hasNext;
} FILEINFO, *PFILEINFO;
