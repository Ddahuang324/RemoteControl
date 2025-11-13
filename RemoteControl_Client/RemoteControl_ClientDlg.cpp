
// RemoteControl_ClientDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "RemoteControl_Client.h"
#include "RemoteControl_ClientDlg.h"
#include "afxdialogex.h"
#include <sstream>      // 【新增】
#include <algorithm>    // 【新增】
#include <cctype>       // 【新增】
#include <set>          // 【新增】
#include <vector>       // 【新增】
#include <cmath>        // 【新增】

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
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

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
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CRemoteControlClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_IPAddress(pDX, IDC_IPADDRESS_Serv, m_Serv_Address);
	DDX_Text(pDX, IDC_EDIT1, m_Port);
	// 绑定控件变量，确保控件 ID 与成员对应
	DDX_Control(pDX, IDC_TREE3, m_Tree);
	DDX_Control(pDX, IDC_LIST4, m_List);
	DDX_Control(pDX, IDC_IPADDRESS_Serv, m_ipAddressServ);
	DDX_Control(pDX, IDC_EDIT1, m_editPort);
	DDX_Control(pDX, IDC_BUTTON2, m_btnViewFileInfo);
}

BEGIN_MESSAGE_MAP(CRemoteControlClientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_TEST, &CRemoteControlClientDlg::OnBnClickedBtnTest)
	ON_BN_CLICKED(IDC_BUTTON2, &CRemoteControlClientDlg::OnBnClickedButton2)
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE3, &CRemoteControlClientDlg::OnTvnSelchangedTree3)
	ON_NOTIFY(NM_DBLCLK, IDC_TREE3, &CRemoteControlClientDlg::OnDblclkTree3)
	// IP 地址控件字段变化通知
	ON_NOTIFY(IPN_FIELDCHANGED, IDC_IPADDRESS_Serv, &CRemoteControlClientDlg::OnIpnFieldchangedIpaddressServ)
	// 端口编辑框文本变化通知
	ON_EN_CHANGE(IDC_EDIT1, &CRemoteControlClientDlg::OnEnChangeEdit1)
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

	// 初始化成员，然后将其写入 UI（成员 -> UI）
	m_Serv_Address = 0x7F000001; // 127.0.0.1
	m_Port = _T("12345");
	UpdateData(FALSE); // 将成员值更新到界面控件

	// 初始化列表控件
	m_List.ModifyStyle(0, LVS_REPORT);
	m_List.SetExtendedStyle(m_List.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
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


// 【重构】OnBnClickedButton1 -> OnBnClickedBtnTest
// 职责：建立长连接，而不是测试。
void CRemoteControlClientDlg::OnBnClickedBtnTest()
{
	UpdateData(TRUE); // UI -> 成员

	DWORD hostIP = m_Serv_Address;
	BYTE* pIP = reinterpret_cast<BYTE*>(&hostIP);
	char ipBuf[16] = {0};
	// 【注意】diff 的 IP 转换是反的 ，P[3] 是第一位
	sprintf_s(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", pIP[3], pIP[2], pIP[1], pIP[0]);
	std::string serverIP(ipBuf);

	int port = _ttoi(m_Port);
	if (port == 0) port = 12345;

	// 使用成员变量 m_clientSocket 建立连接
	if(m_clientSocket.connectToServer(serverIP, (unsigned short)port)){
		MessageBox(L"连接成功！");

		// 发送一个测试包，验证连接
		std::vector<BYTE> data;
		Cpacket packet(CMD::CMD_TEST_CONNECT, data); // 使用现代版的 CMD::CMD_TEST_CONNECT
		if(!m_clientSocket.SendPacket(packet)){
			MessageBox(L"发送测试包失败！");
			m_clientSocket.CloseSocket();
			return;
		}
		std::optional<Cpacket> recvPacket = m_clientSocket.RecvPacket();
		if(!recvPacket || recvPacket->sCmd != CMD::CMD_TEST_CONNECT){
			MessageBox(L"测试包响应失败！");
			m_clientSocket.CloseSocket();
		} else {
			MessageBox(L"服务器响应成功！连接已保持。");
		}
	} else {
		MessageBox(L"连接失败！");
	}
}



void CRemoteControlClientDlg::OnIpnFieldchangedIpaddressServ(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMIPADDRESS pIPAddr = reinterpret_cast<LPNMIPADDRESS>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码

	// 获取IP地址
	BYTE b1, b2, b3, b4;
	m_ipAddressServ.GetAddress(b1, b2, b3, b4);
	CString strIP;
	strIP.Format(L"%d.%d.%d.%d", b1, b2, b3, b4);
	// 简单有效性判断（不为0.0.0.0）
	if (!(b1 == 0 && b2 == 0 && b3 == 0 && b4 == 0)) {
		m_strServerIP = strIP;
	}
	*pResult = 0;
}

void CRemoteControlClientDlg::OnEnChangeEdit1()
{
	CString strPort;
	m_editPort.GetWindowTextW(strPort);
	int port = _ttoi(strPort);
	// 有效端口范围 1-65535
	if (port >= 1 && port <= 65535) {
		m_nServerPort = port;
	}
}

// 查看文件信息按钮点击事件
// 【新增】来自 diff [cite: 739]，但使用长连接和现代协议
void CRemoteControlClientDlg::OnBnClickedButton2()
{
	// 1. 发送请求
	Cpacket packet(CMD::CMD_DRIVER_INFO, {});
	if (!m_clientSocket.SendPacket(packet)) {
		MessageBox(L"发送驱动器请求失败！");
		return;
	}

	// 2. 接收响应
	std::optional<Cpacket> recvPacket = m_clientSocket.RecvPacket();
	if (!recvPacket || recvPacket->sCmd != CMD::CMD_DRIVER_INFO) {
		MessageBox(_T("接收驱动器信息失败！"));
		return;
	}

	// 3. 解析与填充 (逻辑与 diff 基本一致 [cite: 741, 742, 743, 744, 745])
	m_Tree.DeleteAllItems();

	// 【C++17 风格】使用现代 Cpacket 的 std::vector<BYTE> data
	std::string drives;
	if (!recvPacket->data.empty()) {
		drives.assign(recvPacket->data.begin(), recvPacket->data.end());
	} else {
		return; // 没有驱动器
	}

	HTREEITEM hRoot = m_Tree.InsertItem(_T("Drives"));

	for (char &ch : drives) {
		if (ch == ';' || std::isspace(static_cast<unsigned char>(ch))) ch = ',';
	}
	std::set<std::string> seen;
	std::stringstream ss(drives);
	std::string token;

	while (std::getline(ss, token, ',')) {
		// ... (ltrim/rtrim lambda) ...
		// ... (修剪 token) ...
		if (token.empty()) continue;

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
			m_Tree.InsertItem(CString(formatted.c_str()), hRoot);
		}
	}
}
// 【新增】来自 diff [cite: 748]，递归获取路径
CString CRemoteControlClientDlg::GetItemPath(HTREEITEM hItem) {
	if (hItem == NULL) return _T("");
	CString path = m_Tree.GetItemText(hItem);
	HTREEITEM hParent = m_Tree.GetParentItem(hItem);
	while (hParent != NULL) {
		CString parentText = m_Tree.GetItemText(hParent);
		if (parentText != _T("Drives")) {
			// diff 的 bug： "C:\" + "\" + "Windows" -> "C:\\Windows"
			// 应该是 "C:" + "\" + "Windows"
			if (path.Right(1) == _T("\\")) {
				path = parentText + path;
			} else {
				path = parentText + _T("\\") + path;
			}
		} else {
			// 如果父节点是 "Drives"，则停止拼接
			break;
		}
		hParent = m_Tree.GetParentItem(hParent);
	}
	return path;
}
// 【新增】来自 diff [cite: 771]，但使用现代协议
void CRemoteControlClientDlg::OnDblclkTree3(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	HTREEITEM hItem = m_Tree.GetSelectedItem();
	if (hItem == NULL) return;

	if (m_Tree.ItemHasChildren(hItem)) {
		// 清空子项（重新获取）
		HTREEITEM hChild = m_Tree.GetChildItem(hItem);
		while (hChild != NULL) {
			HTREEITEM hNext = m_Tree.GetNextSiblingItem(hChild);
			m_Tree.DeleteItem(hChild);
			hChild = hNext;
		}
	}

	// 1. 获取路径并发包
	CString path = GetItemPath(hItem);
	std::string strPath = CT2A(path);
	std::vector<BYTE> pathData(strPath.begin(), strPath.end());

	Cpacket packet(CMD::CMD_DIRECTORY_INFO, pathData);
	if (!m_clientSocket.SendPacket(packet)) {
		MessageBox(L"发送目录请求失败！");
		return;
	}

	// 2. 【核心重构】进入接收循环，处理流式包
	while (true) {
		std::optional<Cpacket> recvPacket = m_clientSocket.RecvPacket();
		if (!recvPacket) {
			MessageBox(_T("接收数据失败！"));
			break;
		}
		if (recvPacket->sCmd == CMD::CMD_ERROR) {
			std::string errMsg(recvPacket->data.begin(), recvPacket->data.end());
			MessageBox(CString(("错误: " + errMsg).c_str()));
			break;
		} else if (recvPacket->sCmd != CMD::CMD_DIRECTORY_INFO) {
			MessageBox(_T("接收目录信息失败！"));
			break;
		}

		// 3. 【C++17 风格】使用现代反序列化，替换 diff 的 memcpy
		auto finfoOpt = File_Info::Deserialize(recvPacket->data);
		if (!finfoOpt) {
			MessageBox(_T("反序列化数据包失败！"));
			break;
		}
		const File_Info& finfo = *finfoOpt;
		const bool isEndPacket = !finfo.hasNext && !finfo.isDir && finfo.fullPath.empty();
		if (isEndPacket) {
			break; // 结束包来自服务器，停止刷新
		}

		// 4. 过滤并插入 (来自 diff 的逻辑 [cite: 773])
		if (finfo.isDir) {
			m_Tree.InsertItem(CString(finfo.fullPath.c_str()), hItem);
		}
	}
	m_Tree.Expand(hItem, TVE_EXPAND);
}
// 【新增】来自 diff [cite: 750]，但使用现代协议
void CRemoteControlClientDlg::OnTvnSelchangedTree3(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	*pResult = 0;

	HTREEITEM hItem = pNMTreeView->itemNew.hItem;
	if (hItem == NULL) return;

	CString path = GetItemPath(hItem);
	if (path.IsEmpty() || path == _T("Drives")) {
		// ... (清空 m_List 的逻辑，与 diff 相同 [cite: 751, 752])
		m_List.DeleteAllItems();
		CHeaderCtrl* ph = m_List.GetHeaderCtrl();
		if (ph) {
			int colCount = ph->GetItemCount();
			for (int i = colCount - 1; i >= 0; --i) m_List.DeleteColumn(i);
		}
		return;
	}

	// 1. 发送请求
	std::string strPath = CT2A(path);
	std::vector<BYTE> pathData(strPath.begin(), strPath.end());
	Cpacket packet(CMD::CMD_DIRECTORY_INFO, pathData);
	if (!m_clientSocket.SendPacket(packet)) {
		MessageBox(L"发送目录请求失败！");
		return;
	}

	// 2. 【核心重构】收集所有文件名（只收文件）
	std::vector<CString> files;
	while (true) {
		std::optional<Cpacket> recvPacket = m_clientSocket.RecvPacket();
		if (!recvPacket) {
			MessageBox(_T("接收数据失败！"));
			break;
		}
		if (recvPacket->sCmd == CMD::CMD_ERROR) {
			std::string errMsg(recvPacket->data.begin(), recvPacket->data.end());
			MessageBox(CString(("错误: " + errMsg).c_str()));
			break;
		} else if (recvPacket->sCmd != CMD::CMD_DIRECTORY_INFO) {
			MessageBox(_T("接收目录信息失败！"));
			break;
		}

		// 3. 【C++17 风格】使用现代反序列化
		auto finfoOpt = File_Info::Deserialize(recvPacket->data);
		if (!finfoOpt) {
			MessageBox(_T("反序列化数据包失败！"));
			break;
		}
		const File_Info& finfo = *finfoOpt;
		const bool isEndPacket = !finfo.hasNext && !finfo.isDir && finfo.fullPath.empty();
		if (isEndPacket) {
			break; // 到达服务器的结束包
		}

		// 4. 过滤文件 (来自 diff 的逻辑 [cite: 755])
		if (!finfo.isDir) {
			files.push_back(CString(finfo.fullPath.c_str()));
		}
	}

	// 6. 【UI 渲染】准备 List (与 diff 逻辑完全相同 [cite: 758])
	m_List.DeleteAllItems();
	CHeaderCtrl* ph = m_List.GetHeaderCtrl();
	if (ph) {
		int colCount = ph->GetItemCount();
		for (int i = colCount - 1; i >= 0; --i) m_List.DeleteColumn(i);
	}
	if (files.empty()) {
		return;
	}

	// 7. 【UI 渲染】动态多列布局 (与 diff 逻辑完全相同 [cite: 760-770])
	CRect rcClient;
	m_List.GetClientRect(&rcClient);
	CClientDC dc(&m_List);
	CFont* pFont = m_List.GetFont();
	if (pFont) dc.SelectObject(pFont);
	const int padding = 18;

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

	for (int c = 0; c < cols; ++c) {
		m_List.InsertColumn(c, _T(""), LVCFMT_LEFT, 50);
	}

	std::vector<int> colMax(cols, 0);
	for (size_t i = 0; i < files.size(); ++i) {
		int col = (int)(i / rows); // 按列填充
		CSize sz = dc.GetTextExtent(files[i]);
		colMax[col] = max(colMax[col], sz.cx);
	}

	for (int r = 0; r < rows; ++r) {
		bool inserted = false;
		for (int c = 0; c < cols; ++c) {
			int idx = r + c * rows;
			if (idx >= (int)files.size()) continue;
			if (!inserted) {
				m_List.InsertItem(r, files[idx]);
				inserted = true;
			} else {
				m_List.SetItemText(r, c, files[idx]);
			}
		}
	}

	for (int c = 0; c < cols; ++c) {
		m_List.SetColumnWidth(c, colMax[c] + padding);
	}
}
