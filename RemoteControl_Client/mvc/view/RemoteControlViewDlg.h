#pragma once

#include "../interfaces/IController.h"
#include "../model/Interface.h"
#include "../resources/resource_mvc.h"
#include "afxcmn.h"
#include "afxdialogex.h"
#include "afxdlgs.h" // For CFileDialog
#include "afxext.h"  // For CToolBar, CStatusBar
#include "afxwin.h"
#include <memory>
#include <string>
#include <vector>
#include <map>

// 自定义消息 - 用于后台线程通过PostMessage更新UI
#define WM_UPDATE_DRIVE_LIST (WM_APP + 0x101)
#define WM_UPDATE_FILE_LIST (WM_APP + 0x102)
#define WM_UPDATE_SUB_DIRS (WM_APP + 0x103)

// Subdirectory update data structure - use HTREEITEM handle directly
struct SubDirUpdateData {
  HTREEITEM hParent; // Target tree node handle
  std::vector<FileSystemProtocol::FileEntry> entries;
  bool isFirst; // Whether it is the first batch of data (used to clear old
                // nodes)
};

// ============================================================================
// RemoteControlViewDlg: 主对话框View
// 职责:
//   - 显示连接管理、文件浏览、监视控制等UI
//   - 响应用户操作并委托给Controller处理
//   - 接收Controller的更新通知并刷新UI
// ============================================================================
class RemoteControlViewDlg : public CDialogEx {
public:
  // 构造函数: 接收Model接口(用于注册回调等)
  explicit RemoteControlViewDlg(std::shared_ptr<INetworkModel> network,
                                std::shared_ptr<IFileSystemModel> fileSystem,
                                CWnd *pParent = nullptr);

  virtual ~RemoteControlViewDlg();

  // 对话框数据
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_MVC_MAIN_DIALOG };
#endif

  // ---- View接口: 供Controller调用以更新UI ----

  // 更新连接状态
  void UpdateConnectionStatus(bool connected);

  // 更新驱动器列表
  void UpdateDriveList(const std::vector<std::string> &drives);

  // 更新文件列表
  void UpdateFileList(const std::vector<FileSystemProtocol::FileEntry> &files);

  // 更新树控件的子目录列表
  void UpdateSubDirectories(
      HTREEITEM hParent,
      const std::vector<FileSystemProtocol::FileEntry> &entries,
      bool isFirst = true);

  // 显示错误消息
  void ShowError(const std::string &message);

  // 显示信息消息
  void ShowInfo(const std::string &message);

  // 更新状态栏文本
  void UpdateStatusBar(const std::string &text, int pane = 0);

  // 更新传输进度(用于状态栏显示)
  void UpdateTransferProgress(int percent, const std::string &filename);

  // Controller注入
  void SetController(std::shared_ptr<IMainController> controller);

protected:
  virtual void DoDataExchange(CDataExchange *pDX) override;
  virtual BOOL OnInitDialog() override;

  // 消息映射
  DECLARE_MESSAGE_MAP()

  // ---- MFC消息处理函数 ----

  // 系统命令
  afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
  afx_msg void OnPaint();
  afx_msg HCURSOR OnQueryDragIcon();

  // 工具栏按钮
  afx_msg void OnToolbarConnect();
  afx_msg void OnToolbarDisconnect();
  afx_msg void OnToolbarRefresh();
  afx_msg void OnToolbarSettings();
  afx_msg void OnToolbarHelp();
  afx_msg void OnToolbarAbout();

  // 连接区域
  afx_msg void OnBnClickedConnect();
  afx_msg void OnIpAddressChanged(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnPortChanged();

  // 文件浏览区域
  afx_msg void OnBnClickedRefresh();
  afx_msg void OnTreeSelChanged(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnTreeItemExpanding(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnTreeDblClick(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnListRightClick(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnListDblClick(NMHDR *pNMHDR, LRESULT *pResult);

  // 文件操作按钮
  afx_msg void OnBnClickedUpload();
  afx_msg void OnBnClickedDownload();
  afx_msg void OnBnClickedDelete();
  afx_msg void OnBnClickedRun();

  // 右键菜单
  afx_msg void OnMenuDownload();
  afx_msg void OnMenuUpload();
  afx_msg void OnMenuDelete();
  afx_msg void OnMenuRun();
  afx_msg void OnMenuProperties();

  // 监视功能
  afx_msg void OnBnClickedStartMonitor();
  afx_msg void OnBnClickedStartRecord();

  // 窗口大小变化
  afx_msg void OnSize(UINT nType, int cx, int cy);

  // 窗口大小限制
  afx_msg void OnGetMinMaxInfo(MINMAXINFO *lpMMI);

  // 后台线程通过 PostMessage 通知网络状态变化
  afx_msg LRESULT OnNetworkStatusChanged(WPARAM wParam, LPARAM lParam);

  // 后台线程通过 PostMessage 更新驱动器列表
  afx_msg LRESULT OnUpdateDriveList(WPARAM wParam, LPARAM lParam);

  // 后台线程通过 PostMessage 更新文件列表
  afx_msg LRESULT OnUpdateFileList(WPARAM wParam, LPARAM lParam);

  // 后台线程通过 PostMessage 更新子目录
  afx_msg LRESULT OnUpdateSubDirs(WPARAM wParam, LPARAM lParam);

private:
  // ===== 响应式布局支持 =====
  struct ControlAnchor {
    int nID; // 控件ID
    bool anchorLeft;
    bool anchorTop;
    bool anchorRight;
    bool anchorBottom;
  };

  std::vector<ControlAnchor> m_layoutRules; // 布局规则
  CRect m_rcOriginalRect;                    // OnInitDialog 时的客户区
  std::map<int, CRect> m_originalControlRects; // 每个受控件的初始 rect

  // 调整单个控件布局（在 OnSize 中调用）
  void AdjustControlLayout(int cx, int cy);

  // ---- Model接口 ----
  std::shared_ptr<INetworkModel> network_;
  std::shared_ptr<IFileSystemModel> fileSystem_;

  // ---- Controller ----
  std::shared_ptr<IMainController> controller_;

  // ---- UI控件 ----

  // 工具栏和状态栏
  CToolBar m_toolbar;
  CStatusBar m_statusBar;

  // 连接区域
  CStatic m_staticIP;
  CIPAddressCtrl m_ipAddress;
  CStatic m_staticPort;
  CEdit m_editPort;
  CButton m_btnConnect;
  CStatic m_staticStatus;
  CStatic m_ledStatus; // 状态指示灯(自绘)

  // 文件管理区域
  CTreeCtrl m_treeDrives;
  CListCtrl m_listFiles;
  CEdit m_editPath; // 地址栏
  CButton m_btnRefresh;
  CButton m_btnUpload;
  CButton m_btnDownload;
  CButton m_btnDelete;
  CButton m_btnRun;
  // 监视区域
  CButton m_btnStartMonitor;
  CButton m_btnStartRecord;

  // 分组框
  CButton m_groupConnection;
  CButton m_groupFiles;
  CButton m_groupMonitor;

  // ---- 内部状态 ----
  HICON m_hIcon;
  bool m_bConnected;
  bool m_bMonitoring;        // 屏幕监视状态
  bool m_bFileListCleared;   // 标记文件列表是否已为新目录清空
  CString m_strCurrentPath;  // 当前浏览的路径
  CString m_strSelectedFile; // 当前选中的文件

  // ---- Debounce state ----
  CString m_strLastSelectedPath; // Last selected path (for OnTreeSelChanged
                                 // debounce)

  // ---- 辅助方法 ----

  // 初始化工具栏
  void InitToolbar();

  // 初始化状态栏
  void InitStatusBar();

  // 初始化文件列表控件(设置列)
  void InitFileList();

  // 递归获取树节点的完整路径
  CString GetTreeItemPath(HTREEITEM hItem);

  // 更新按钮状态(根据连接状态启用/禁用)
  void UpdateButtonStates();

  // 自绘状态指示灯
  void DrawStatusLED(CDC *pDC, CRect rect, bool connected);
};
