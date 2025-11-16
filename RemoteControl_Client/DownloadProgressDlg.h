// DownloadProgressDlg.h: 头文件
//

#pragma once

// CDownloadProgressDlg 对话框

class CDownloadProgressDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CDownloadProgressDlg)

public:
    CDownloadProgressDlg(CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~CDownloadProgressDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DOWNLOAD_PROGRESS_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
    HICON m_hIcon;
    CProgressCtrl m_progress;

    // 生成的消息映射函数
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    DECLARE_MESSAGE_MAP()
public:
    void SetProgress(int progress);
    void SetMessage(const CString& msg);
};
