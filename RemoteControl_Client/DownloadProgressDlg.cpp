// DownloadProgressDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "RemoteControl_Client.h"
#include "DownloadProgressDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CDownloadProgressDlg 对话框

IMPLEMENT_DYNAMIC(CDownloadProgressDlg, CDialogEx)

CDownloadProgressDlg::CDownloadProgressDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_DOWNLOAD_PROGRESS_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

CDownloadProgressDlg::~CDownloadProgressDlg()
{
}

void CDownloadProgressDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_PROGRESS_DOWNLOAD, m_progress);
}

BEGIN_MESSAGE_MAP(CDownloadProgressDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
END_MESSAGE_MAP()


// CDownloadProgressDlg 消息处理程序

BOOL CDownloadProgressDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
    //  执行此操作
    SetIcon(m_hIcon, TRUE);            // 设置大图标
    SetIcon(m_hIcon, FALSE);        // 设置小图标

    // 初始化进度条
    m_progress.SetRange(0, 100);
    m_progress.SetPos(0);

    return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CDownloadProgressDlg::OnPaint()
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
HCURSOR CDownloadProgressDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CDownloadProgressDlg::SetProgress(int progress)
{
    m_progress.SetPos(progress);
}

void CDownloadProgressDlg::SetMessage(const CString& msg)
{
    SetWindowText(msg);
}
