
// RemoteControl_ClientDlg.h: 头文件
//

#pragma once
#include "clientSocket.h"
#include <string>
#include <vector>
#include <optional>
#include "Enities.h"

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

		CString m_strServerIP = L"127.0.0.1"; // 当前服务器IP
		int m_nServerPort = 9527; // 当前服务器端口，默认9527

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
		DECLARE_MESSAGE_MAP()
};
