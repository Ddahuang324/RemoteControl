
// RemoteControl_ClientDlg.h: 头文件
//

#pragma once
#include "clientSocket.h"

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

	// 生成的消息映射函数
	virtual BOOL OnInitDialog() override;
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedBtnTest();
	afx_msg void OnBnClickedButton2();
	DWORD m_Serv_Address;
	CString m_Port;
	afx_msg void OnTvnSelchangedTree3(NMHDR* pNMHDR, LRESULT* pResult);
	CTreeCtrl m_Tree;
	CString GetSelectedDrive() const;
	CString GetItemPath(HTREEITEM hItem);
	afx_msg void OnDblclkTree3(NMHDR* pNMHDR, LRESULT* pResult);
	CListCtrl m_List;
};

typedef struct file_info {
    bool isValid;
    bool isDir;
    char szFileName[256];
    bool hasNext;
} FILEINFO, *PFILEINFO;
