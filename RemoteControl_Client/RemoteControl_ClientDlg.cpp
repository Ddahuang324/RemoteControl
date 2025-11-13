// RemoteControl_ClientDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "RemoteControl_Client.h"
#include "RemoteControl_ClientDlg.h"
#include "afxdialogex.h"
#include "resource.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <set>
#include <commctrl.h>
#include <vector>
#include <cmath>
#include <windows.h>
#include <shellapi.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CRemoteControlClientDlg 对话框



CRemoteControlClientDlg::CRemoteControlClientDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_REMOTECONTROL_CLIENT_DIALOG, pParent)
	, m_Serv_Address(0)
	, m_Port(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CRemoteControlClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_IPAddress(pDX, IDC_IPADDRESS_Serv, m_Serv_Address);
	DDX_Text(pDX, IDC_EDIT1, m_Port);
	DDX_Control(pDX, IDC_TREE3, m_Tree);
	DDX_Control(pDX, IDC_LIST4, m_List);
}







BEGIN_MESSAGE_MAP(CRemoteControlClientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_TEST, &CRemoteControlClientDlg::OnBnClickedBtnTest)
	ON_BN_CLICKED(IDC_BUTTON2, &CRemoteControlClientDlg::OnBnClickedButton2)
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE3, &CRemoteControlClientDlg::OnTvnSelchangedTree3)
	ON_NOTIFY(NM_DBLCLK, IDC_TREE3, &CRemoteControlClientDlg::OnDblclkTree3)
END_MESSAGE_MAP()


// CRemoteControlClientDlg 消息处理程序

BOOL CRemoteControlClientDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	UpdateData();
	m_Serv_Address = 0x7F000001; // 默认IP地址
	m_Port = _T("12345");
	UpdateData(FALSE);

	// 初始化列表控件
	// 切换到 Report 视图以支持多列显示
	m_List.ModifyStyle(0, LVS_REPORT);
	m_List.InsertColumn(0, _T("文件名"), LVCFMT_LEFT, 150);
	m_List.SetExtendedStyle(m_List.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}


bool CRemoteControlClientDlg::SendCommandPacket(int nCommand, const BYTE* pData, size_t nSize){
	
	UpdateData();


	DWORD hostIP = m_Serv_Address;
	BYTE* pIP = reinterpret_cast<BYTE*>(&hostIP);
	char ipBuf[16] = {0};
	sprintf_s(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", pIP[3], pIP[2], pIP[1], pIP[0]);
	std::string serverIP(ipBuf);

	// 获取端口（m_Port 是 CString，可能为宽字符）
	int port = _ttoi(m_Port);
	if (port == 0) port = 12345
	; // 使用默认端口
	CClientSocket& clientSocket = CClientSocket::GetInstance();
	bool bRet = clientSocket.initSocket(serverIP, port);
	
	if(!bRet ){
		MessageBox(_T("连接失败！"));
		return false;
	}

	Cpacket pkt(nCommand, pData, nSize);

	bRet = clientSocket.Send(pkt);
	if(bRet == false){
		MessageBox(_T("发送失败！"));
		clientSocket.CloseSocket();
		return false;
	}

	return true;
}


void CRemoteControlClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CRemoteControlClientDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CRemoteControlClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}




void CRemoteControlClientDlg::OnBnClickedBtnTest()
{
	if (!SendCommandPacket(2002)) return;
	CClientSocket& clientSocket = CClientSocket::GetInstance();
	int cmd = clientSocket.DealCommand();
	if (cmd == 2002) {
		MessageBox(_T("测试连接成功！"));
	} else {
		MessageBox(_T("接收失败！"));
	}
	clientSocket.CloseSocket();
}


void CRemoteControlClientDlg::OnBnClickedButton2()
{
	if (!SendCommandPacket(1)) return;
	
	CClientSocket& clientSocket = CClientSocket::GetInstance();
	int cmd = clientSocket.DealCommand();
	if (cmd != 1) {
		MessageBox(_T("接收失败！"));
		clientSocket.CloseSocket();
		return;
	}
	
	m_Tree.DeleteAllItems();
	Cpacket& packet = clientSocket.GetPacket();
	if (packet.sCmd == 1) {
		std::string drives = packet.strData;
		HTREEITEM hRoot = m_Tree.InsertItem(_T("Drives"));

		// 规范化分隔符：把分号和空白转换为逗号，然后按逗号分割
		for (char &ch : drives) {
			if (ch == ';' || std::isspace(static_cast<unsigned char>(ch))) ch = ',';
		}

		std::set<std::string> seen; // 去重

		std::stringstream ss(drives);
		std::string token;
		while (std::getline(ss, token, ',')) {
			// 修剪前后空白
			auto ltrim = [] (std::string &s) {
				while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
			};
			auto rtrim = [] (std::string &s) {
				while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
			};
			ltrim(token); rtrim(token);

			if (token.empty()) continue;

			// 提取驱动器字母（第一个字母字符），并格式化为 "X:\"
			char letter = '\0';
			for (char c : token) {
				if (std::isalpha(static_cast<unsigned char>(c))) {
					letter = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
					break;
				}
			}
			if (letter == '\0') continue;

			std::string formatted;
			formatted.push_back(letter);
			formatted += ":\\";

			if (seen.insert(formatted).second) {
				// CString 构造在此项目可接受 std::string.c_str()
				m_Tree.InsertItem(CString(formatted.c_str()), hRoot);
			}
		}
	}
	clientSocket.CloseSocket();
}

CString CRemoteControlClientDlg::GetSelectedDrive() const {
	HTREEITEM hItem = m_Tree.GetSelectedItem();
	if (hItem == NULL) return _T("");
	return m_Tree.GetItemText(hItem);
}

CString CRemoteControlClientDlg::GetItemPath(HTREEITEM hItem) {
	if (hItem == NULL) return _T("");
	CString path = m_Tree.GetItemText(hItem);
	HTREEITEM hParent = m_Tree.GetParentItem(hItem);
	while (hParent != NULL) {
		CString parentText = m_Tree.GetItemText(hParent);
		if (parentText != _T("Drives")) {
			path = parentText + _T("\\") + path;
		}
		hParent = m_Tree.GetParentItem(hParent);
	}
	return path;
}

void CRemoteControlClientDlg::OnTvnSelchangedTree3(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	HTREEITEM hItem = pNMTreeView->itemNew.hItem;
	if (hItem == NULL) return;

	CString path = GetItemPath(hItem);
	if (path == _T("Drives")) {
		// 清空列表和列
		m_List.DeleteAllItems();
		CHeaderCtrl* ph = m_List.GetHeaderCtrl();
		if (ph) {
			int colCount = ph->GetItemCount();
			for (int i = colCount - 1; i >= 0; --i) m_List.DeleteColumn(i);
		}
		return;
	}

	std::string strPath = CT2A(path);
	if (!SendCommandPacket(2, reinterpret_cast<const BYTE*>(strPath.c_str()), strPath.size())) return;

	CClientSocket& clientSocket = CClientSocket::GetInstance();
	// 收集所有文件名（只收文件，不要目录）
	std::vector<CString> files;
	while (true) {
		int cmd = clientSocket.DealCommand();
		if (cmd != 2) {
			MessageBox(_T("接收失败！"));
			break;
		}
		Cpacket& packet = clientSocket.GetPacket();
		if (packet.strData.size() != sizeof(FILEINFO)) {
			MessageBox(_T("数据格式错误！"));
			break;
		}
		FILEINFO finfo;
		memcpy(&finfo, packet.strData.data(), sizeof(finfo));
		if (finfo.isValid && !finfo.isDir) {
#ifdef UNICODE
			int len = MultiByteToWideChar(CP_ACP, 0, finfo.szFileName, -1, NULL, 0);
			CString name;
			if (len > 0) {
				std::vector<wchar_t> buf(len);
				MultiByteToWideChar(CP_ACP, 0, finfo.szFileName, -1, buf.data(), len);
				name = buf.data();
			}
#else
			CString name = CString(finfo.szFileName);
#endif
			files.push_back(name);
		}
		if (!finfo.hasNext) break;
	}

	// 准备 List：删除已有列与项
	m_List.DeleteAllItems();
	CHeaderCtrl* ph = m_List.GetHeaderCtrl();
	if (ph) {
		int colCount = ph->GetItemCount();
		for (int i = colCount - 1; i >= 0; --i) m_List.DeleteColumn(i);
	}

	if (files.empty()) {
		clientSocket.CloseSocket();
		return;
	}

	// 为了按列显示，先测量文本宽度并计算列数
	CRect rcClient;
	m_List.GetClientRect(&rcClient);
	CClientDC dc(&m_List);
	CFont* pFont = m_List.GetFont();
	if (pFont) dc.SelectObject(pFont);
	const int padding = 18; // 列内额外像素

	int maxNameWidth = 0;
	for (auto &s : files) {
		CSize sz = dc.GetTextExtent(s);
		maxNameWidth = max(maxNameWidth, sz.cx);
	}

	int clientW = rcClient.Width();
	int cols = 1;
	if (maxNameWidth + padding >= clientW) cols = 1;
	else cols = min((int)files.size(), max(1, clientW / (maxNameWidth + padding)));
	int rows = (int)ceil((double)files.size() / cols);

	// 插入列（标题可以空或简短）
	for (int c = 0; c < cols; ++c) {
		CString hdr;
		hdr.Format(_T(""));
		m_List.InsertColumn(c, hdr, LVCFMT_LEFT, 50);
	}

	// 计算每列的最大宽度（像素）
	std::vector<int> colMax(cols, 0);
	for (size_t i = 0; i < files.size(); ++i) {
		int row = (int)(i % rows);
		int col = (int)(i / rows);
		CSize sz = dc.GetTextExtent(files[i]);
		colMax[col] = max(colMax[col], sz.cx);
	}

	// 插入行并填充多列：按列主（column-major）分配，使列高度尽量均衡
	for (int r = 0; r < rows; ++r) {
		bool inserted = false;
		for (int c = 0; c < cols; ++c) {
			int idx = r + c * rows;
			if (idx >= (int)files.size()) continue;
			if (!inserted) {
				// 在该行插入第一列
				m_List.InsertItem(r, files[idx]);
				inserted = true;
			} else {
				// 设置子项文本
				m_List.SetItemText(r, c, files[idx]);
			}
		}
	}

	// 设置每列宽度（包含 padding）
	for (int c = 0; c < cols; ++c) {
		int w = colMax[c] + padding;
		m_List.SetColumnWidth(c, w);
	}

	clientSocket.CloseSocket();
}

void CRemoteControlClientDlg::OnDblclkTree3(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	HTREEITEM hItem = m_Tree.GetSelectedItem();
	if (hItem == NULL) return;

	if (m_Tree.ItemHasChildren(hItem)) {
		m_Tree.Expand(hItem, TVE_TOGGLE);
	} else {
		CString path = GetItemPath(hItem);
		std::string strPath = CT2A(path);
		if (!SendCommandPacket(2, reinterpret_cast<const BYTE*>(strPath.c_str()), strPath.size())) return;
		CClientSocket& clientSocket = CClientSocket::GetInstance();
		while (true) {
			int cmd = clientSocket.DealCommand();
			if (cmd != 2) {
				MessageBox(_T("接收失败！"));
				break;
			}
			Cpacket& packet = clientSocket.GetPacket();
			if (packet.strData.size() != sizeof(FILEINFO)) {
				MessageBox(_T("数据格式错误！"));
				break;
			}
			FILEINFO finfo;
			memcpy(&finfo, packet.strData.data(), sizeof(finfo));
			if (finfo.isValid && finfo.isDir) {
				m_Tree.InsertItem(CString(finfo.szFileName), hItem);
			}
			if (!finfo.hasNext) break;
		}
		m_Tree.Expand(hItem, TVE_EXPAND);
		clientSocket.CloseSocket();
	}
}
