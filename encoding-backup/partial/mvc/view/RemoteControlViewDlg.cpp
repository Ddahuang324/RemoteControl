#include "pch.h"
#include "RemoteControlViewDlg.h"
#include "afxdialogex.h"


#include <sstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ============================================================================
// 构造与析构
// ============================================================================

RemoteControlViewDlg::RemoteControlViewDlg(
    std::shared_ptr<INetworkModel> network,
    std::shared_ptr<IFileSystemModel> fileSystem, CWnd *pParent)
    : CDialogEx(IDD_MVC_MAIN_DIALOG, pParent), network_(network),
      fileSystem_(fileSystem), controller_(nullptr), m_bConnected(false) {
  // 暂时使用默认图标，稍后添加应用程序图标
  m_hIcon = nullptr;
}

RemoteControlViewDlg::~RemoteControlViewDlg() {
  // 释放所有树节点中存储的路径数据
  if (m_treeDrives.GetSafeHwnd()) {
    HTREEITEM hRoot = m_treeDrives.GetRootItem();
    if (hRoot != NULL) {
      std::function<void(HTREEITEM)> freeItemData = [&](HTREEITEM hItem) {
        HTREEITEM hChild = m_treeDrives.GetChildItem(hItem);
        while (hChild != NULL) {
          freeItemData(hChild);
          HTREEITEM hNext = m_treeDrives.GetNextSiblingItem(hChild);
          DWORD_PTR data = m_treeDrives.GetItemData(hChild);
          if (data != 0) {
            delete reinterpret_cast<CString *>(data);
          }
          hChild = hNext;
        }
      };
      freeItemData(hRoot);
    }
  }
}

void RemoteControlViewDlg::DoDataExchange(CDataExchange *pDX) {
  CDialogEx::DoDataExchange(pDX);

  // ===== 临时简化版本 - 用于排查问题 =====
  // 只绑定最基本的控件,逐步测试

  // 测试 1: 只绑定按钮
  // 网络连接区域
  DDX_Control(pDX, IDC_MVC_BTN_CONNECT, m_btnConnect);

  DDX_Control(pDX, IDC_MVC_STATIC_IP, m_staticIP);
  DDX_Control(pDX, IDC_MVC_IPADDRESS, m_ipAddress);
  DDX_Control(pDX, IDC_MVC_STATIC_PORT, m_staticPort);
  DDX_Control(pDX, IDC_MVC_EDIT_PORT, m_editPort);
  DDX_Control(pDX, IDC_MVC_STATIC_STATUS, m_staticStatus);

  // 文件管理区域
  DDX_Control(pDX, IDC_MVC_TREE_DRIVES, m_treeDrives);
  DDX_Control(pDX, IDC_MVC_LIST_FILES, m_listFiles);
  DDX_Control(pDX, IDC_MVC_EDIT_PATH, m_editPath);
  DDX_Control(pDX, IDC_MVC_BTN_REFRESH, m_btnRefresh);
  DDX_Control(pDX, IDC_MVC_BTN_UPLOAD, m_btnUpload);
  DDX_Control(pDX, IDC_MVC_BTN_DOWNLOAD, m_btnDownload);
  DDX_Control(pDX, IDC_MVC_BTN_DELETE, m_btnDelete);
  DDX_Control(pDX, IDC_MVC_BTN_RUN, m_btnRun);

  // 监视区域
  DDX_Control(pDX, IDC_MVC_BTN_START_MONITOR, m_btnStartMonitor);
  DDX_Control(pDX, IDC_MVC_BTN_START_RECORD, m_btnStartRecord);

  // 分组框
  DDX_Control(pDX, IDC_MVC_GROUP_CONNECTION, m_groupConnection);
  DDX_Control(pDX, IDC_MVC_GROUP_FILES, m_groupFiles);
  DDX_Control(pDX, IDC_MVC_GROUP_MONITOR, m_groupMonitor);
}

BEGIN_MESSAGE_MAP(RemoteControlViewDlg, CDialogEx)
ON_WM_SYSCOMMAND()
ON_WM_PAINT()
ON_WM_QUERYDRAGICON()
ON_WM_SIZE()
ON_MESSAGE(WM_APP + 0x100, &RemoteControlViewDlg::OnNetworkStatusChanged)
ON_MESSAGE(WM_UPDATE_DRIVE_LIST, &RemoteControlViewDlg::OnUpdateDriveList)
ON_MESSAGE(WM_UPDATE_FILE_LIST, &RemoteControlViewDlg::OnUpdateFileList)
ON_MESSAGE(WM_UPDATE_SUB_DIRS, &RemoteControlViewDlg::OnUpdateSubDirs)

// 工具栏
ON_COMMAND(ID_MVC_TOOLBAR_CONNECT, &RemoteControlViewDlg::OnToolbarConnect)
ON_COMMAND(ID_MVC_TOOLBAR_DISCONNECT,
           &RemoteControlViewDlg::OnToolbarDisconnect)
ON_COMMAND(ID_MVC_TOOLBAR_REFRESH, &RemoteControlViewDlg::OnToolbarRefresh)
ON_COMMAND(ID_MVC_TOOLBAR_SETTINGS, &RemoteControlViewDlg::OnToolbarSettings)
ON_COMMAND(ID_MVC_TOOLBAR_HELP, &RemoteControlViewDlg::OnToolbarHelp)
ON_COMMAND(ID_MVC_TOOLBAR_ABOUT, &RemoteControlViewDlg::OnToolbarAbout)

// 连接按钮
ON_BN_CLICKED(IDC_MVC_BTN_CONNECT, &RemoteControlViewDlg::OnBnClickedConnect)
ON_NOTIFY(IPN_FIELDCHANGED, IDC_MVC_IPADDRESS,
          &RemoteControlViewDlg::OnIpAddressChanged)
ON_EN_CHANGE(IDC_MVC_EDIT_PORT, &RemoteControlViewDlg::OnPortChanged)

// 文件浏览
ON_BN_CLICKED(IDC_MVC_BTN_REFRESH, &RemoteControlViewDlg::OnBnClickedRefresh)
ON_NOTIFY(TVN_SELCHANGED, IDC_MVC_TREE_DRIVES,
          &RemoteControlViewDlg::OnTreeSelChanged)
ON_NOTIFY(TVN_ITEMEXPANDING, IDC_MVC_TREE_DRIVES,
          &RemoteControlViewDlg::OnTreeItemExpanding)
ON_NOTIFY(NM_DBLCLK, IDC_MVC_TREE_DRIVES, &RemoteControlViewDlg::OnTreeDblClick)
ON_NOTIFY(NM_RCLICK, IDC_MVC_LIST_FILES,
          &RemoteControlViewDlg::OnListRightClick)
ON_NOTIFY(NM_DBLCLK, IDC_MVC_LIST_FILES, &RemoteControlViewDlg::OnListDblClick)

// 文件操作按钮
ON_BN_CLICKED(IDC_MVC_BTN_UPLOAD, &RemoteControlViewDlg::OnBnClickedUpload)
ON_BN_CLICKED(IDC_MVC_BTN_DOWNLOAD, &RemoteControlViewDlg::OnBnClickedDownload)
ON_BN_CLICKED(IDC_MVC_BTN_DELETE, &RemoteControlViewDlg::OnBnClickedDelete)
ON_BN_CLICKED(IDC_MVC_BTN_RUN, &RemoteControlViewDlg::OnBnClickedRun)

// 右键菜单
ON_COMMAND(ID_MVC_MENU_DOWNLOAD, &RemoteControlViewDlg::OnMenuDownload)
ON_COMMAND(ID_MVC_MENU_UPLOAD, &RemoteControlViewDlg::OnMenuUpload)
ON_COMMAND(ID_MVC_MENU_DELETE, &RemoteControlViewDlg::OnMenuDelete)
ON_COMMAND(ID_MVC_MENU_RUN, &RemoteControlViewDlg::OnMenuRun)
ON_COMMAND(ID_MVC_MENU_PROPERTIES, &RemoteControlViewDlg::OnMenuProperties)

// 监视功能
ON_BN_CLICKED(IDC_MVC_BTN_START_MONITOR,
              &RemoteControlViewDlg::OnBnClickedStartMonitor)
ON_BN_CLICKED(IDC_MVC_BTN_START_RECORD,
              &RemoteControlViewDlg::OnBnClickedStartRecord)
END_MESSAGE_MAP()

// ============================================================================
// 初始化
// ============================================================================

BOOL RemoteControlViewDlg::OnInitDialog() {
  CDialogEx::OnInitDialog();

  // 设置图标
  // TODO: 加载实际的应用程序图标
  // SetIcon(m_hIcon, TRUE);
  // SetIcon(m_hIcon, FALSE);

  // 初始化工具栏
  InitToolbar();

  // 初始化状态栏
  InitStatusBar();

  // 初始化文件列表
  InitFileList();

  // 设置默认IP和端口(在控件成功创建后再写入,避免调试版断言)
  if (m_ipAddress.GetSafeHwnd()) {
    m_ipAddress.SetAddress(127, 0, 0, 1);
  } else {
    TRACE0("Warning: IP address control is not available, skip default "
           "address.\n");
  }
  if (m_editPort.GetSafeHwnd()) {
    m_editPort.SetWindowText(_T("9527"));
  }

  // 初始化按钮状态
  UpdateButtonStates();

  // 设置状态栏初始文本
  UpdateStatusBar("就绪", 0);

  return TRUE;
}

// 接收来自后台线程的网络状态通知 (wParam: connected)
LRESULT RemoteControlViewDlg::OnNetworkStatusChanged(WPARAM wParam,
                                                     LPARAM lParam) {
  bool connected = (wParam != 0);
  UpdateConnectionStatus(connected);
  if (connected) {
    ShowInfo("连接成功!");
    UpdateStatusBar("已连接", 0);
  } else {
    ShowError("连接失败,请检查IP和端口是否正确");
    UpdateStatusBar("连接失败", 0);
  }
  return 0;
}

// 接收来自后台线程的驱动器列表更新 (wParam: 数据指针)
LRESULT RemoteControlViewDlg::OnUpdateDriveList(WPARAM wParam, LPARAM lParam) {
  auto *pDrives = reinterpret_cast<std::vector<std::string> *>(wParam);
  if (pDrives) {
    UpdateDriveList(*pDrives);
    UpdateStatusBar("驱动器列表已刷新", 0);
    delete pDrives; // 释放内存
  }
  return 0;
}

// 接收来自后台线程的文件列表更新 (wParam: 数据指针, lParam: hasMore)
LRESULT RemoteControlViewDlg::OnUpdateFileList(WPARAM wParam, LPARAM lParam) {
  auto *pEntries =
      reinterpret_cast<std::vector<FileSystemProtocol::FileEntry> *>(wParam);
  bool hasMore = (bool)lParam;

  if (pEntries) {
    UpdateFileList(*pEntries);
    if (!hasMore) {
      UpdateStatusBar("目录加载完成", 0);
    }
    delete pEntries; // 释放内存
  }
  return 0;
}

// 接收来自后台线程的子目录更新 (wParam: SubDirUpdateData指针)
LRESULT RemoteControlViewDlg::OnUpdateSubDirs(WPARAM wParam, LPARAM lParam) {
  auto *pData = reinterpret_cast<SubDirUpdateData *>(wParam);

  if (pData) {
    // 验证树节点句柄是否仍然有效
    // 在异步回调期间，用户可能已经删除或修改了树节点
    if (pData->hParent != NULL) {
      // 尝试获取节点路径，如果失败说明节点已被删除
      CString testPath = GetTreeItemPath(pData->hParent);
      if (!testPath.IsEmpty()) {
        // 节点仍然有效，更新子目录
        TRACE(
            _T("OnUpdateSubDirs: hParent is valid, updating subdirectories\n"));
        UpdateSubDirectories(pData->hParent, pData->entries);
        UpdateStatusBar("子目录加载完成", 0);
      } else {
        // 节点已失效，记录日志但不崩溃
        TRACE(_T("OnUpdateSubDirs: hParent is invalid (node deleted), ")
              _T("skipping update\n"));
      }
    } else {
      TRACE(_T("OnUpdateSubDirs: hParent is NULL\n"));
    }
    delete pData; // 释放内存
  }
  return 0;
}

void RemoteControlViewDlg::InitToolbar() {
  // 创建工具栏
  if (!m_toolbar.CreateEx(this, TBSTYLE_FLAT,
                          WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS |
                              CBRS_FLYBY)) {
    TRACE0("Failed to create toolbar\n");
    return;
  }

  // 设置工具栏按钮(简化版,实际应加载位图)
  TBBUTTON buttons[] = {
      {0,
       ID_MVC_TOOLBAR_CONNECT,
       TBSTATE_ENABLED,
       TBSTYLE_BUTTON,
       {0},
       0,
       (INT_PTR) _T("连接")},
      {1,
       ID_MVC_TOOLBAR_DISCONNECT,
       TBSTATE_ENABLED,
       TBSTYLE_BUTTON,
       {0},
       0,
       (INT_PTR) _T("断开")},
      {0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0},
      {2,
       ID_MVC_TOOLBAR_REFRESH,
       TBSTATE_ENABLED,
       TBSTYLE_BUTTON,
       {0},
       0,
       (INT_PTR) _T("刷新")},
      {0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0},
      {3,
       ID_MVC_TOOLBAR_SETTINGS,
       TBSTATE_ENABLED,
       TBSTYLE_BUTTON,
       {0},
       0,
       (INT_PTR) _T("设置")},
      {4,
       ID_MVC_TOOLBAR_HELP,
       TBSTATE_ENABLED,
       TBSTYLE_BUTTON,
       {0},
       0,
       (INT_PTR) _T("帮助")},
  };

  m_toolbar.GetToolBarCtrl().AddButtons(_countof(buttons), buttons);
  m_toolbar.SetButtonText(0, _T("连接"));
  m_toolbar.SetButtonText(1, _T("断开"));
  m_toolbar.SetButtonText(2, _T("刷新"));
  m_toolbar.SetButtonText(3, _T("设置"));
  m_toolbar.SetButtonText(4, _T("帮助"));

  // 重新计算布局
  RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
}

void RemoteControlViewDlg::InitStatusBar() {
  // 创建状态栏
  if (!m_statusBar.Create(this)) {
    TRACE0("Failed to create status bar\n");
    return;
  }

  // 设置状态栏分栏
  static UINT indicators[] = {
      ID_SEPARATOR, // 状态文本
      ID_SEPARATOR, // 传输速度
      ID_SEPARATOR, // 网络延迟
  };

  m_statusBar.SetIndicators(indicators, _countof(indicators));
  m_statusBar.SetPaneInfo(0, ID_SEPARATOR, SBPS_STRETCH, 0);
  m_statusBar.SetPaneInfo(1, ID_SEPARATOR, SBPS_NORMAL, 150);
  m_statusBar.SetPaneInfo(2, ID_SEPARATOR, SBPS_NORMAL, 100);

  m_statusBar.SetPaneText(0, _T("就绪"));
  m_statusBar.SetPaneText(1, _T("传输: 0 KB/s"));
  m_statusBar.SetPaneText(2, _T("延迟: 0 ms"));

  // 重新计算布局
  RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
}

void RemoteControlViewDlg::InitFileList() {
  // 设置列表控件为详细信息模式
  m_listFiles.ModifyStyle(0, LVS_REPORT);
  m_listFiles.SetExtendedStyle(m_listFiles.GetExtendedStyle() |
                               LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  // 添加列
  m_listFiles.InsertColumn(0, _T("名称"), LVCFMT_LEFT, 200);
  m_listFiles.InsertColumn(1, _T("大小"), LVCFMT_RIGHT, 100);
  m_listFiles.InsertColumn(2, _T("类型"), LVCFMT_LEFT, 100);
  m_listFiles.InsertColumn(3, _T("修改时间"), LVCFMT_LEFT, 150);
}

// ============================================================================
// Controller注入
// ============================================================================

void RemoteControlViewDlg::SetController(
    std::shared_ptr<IMainController> controller) {
  controller_ = controller;
}

// ============================================================================
// View接口实现: 供Controller调用以更新UI
// ============================================================================

void RemoteControlViewDlg::UpdateConnectionStatus(bool connected) {
  m_bConnected = connected;

  // 更新按钮文本
  if (m_btnConnect.GetSafeHwnd()) {
    m_btnConnect.SetWindowText(connected ? _T("断开连接") : _T("连接"));
  }

  // 更新状态文本
  if (m_staticStatus.GetSafeHwnd()) {
    m_staticStatus.SetWindowText(connected ? _T("已连接") : _T("未连接"));
  }

  // 触发状态指示灯重绘
  CWnd *pLed = GetDlgItem(IDC_MVC_LED_STATUS);
  if (pLed && pLed->GetSafeHwnd()) {
    pLed->Invalidate();
  }

  // 更新按钮状态
  UpdateButtonStates();

  // 更新状态栏
  UpdateStatusBar(connected ? "已连接到服务器" : "未连接", 0);
}

void RemoteControlViewDlg::UpdateDriveList(
    const std::vector<std::string> &drives) {
  // 清空树控件前,先释放所有存储的路径数据
  HTREEITEM hRoot = m_treeDrives.GetRootItem();
  if (hRoot != NULL) {
    std::function<void(HTREEITEM)> freeItemData = [&](HTREEITEM hItem) {
      HTREEITEM hChild = m_treeDrives.GetChildItem(hItem);
      while (hChild != NULL) {
        freeItemData(hChild);
        HTREEITEM hNext = m_treeDrives.GetNextSiblingItem(hChild);
        DWORD_PTR data = m_treeDrives.GetItemData(hChild);
        if (data != 0) {
          delete reinterpret_cast<CString *>(data);
        }
        hChild = hNext;
      }
    };
    freeItemData(hRoot);
  }

  // 清空树控件
  m_treeDrives.DeleteAllItems();

  // 插入根节点
  hRoot = m_treeDrives.InsertItem(_T("驱动器"));

  // 插入每个驱动器
  for (const auto &drive : drives) {
    CString driveName(drive.c_str());
    HTREEITEM hDrive = m_treeDrives.InsertItem(driveName, hRoot);

    // 修复：为驱动器节点也存储完整路径到 ItemData
    // 这确保所有节点（驱动器和子目录）都使用相同的路径存储机制
    CString *pDrivePath = new CString(drive.c_str());
    m_treeDrives.SetItemData(hDrive, reinterpret_cast<DWORD_PTR>(pDrivePath));

    // 为每个磁盘添加一个虚拟子节点(占位符)
    // 这样树控件会显示展开按钮，用户可以点击展开
    m_treeDrives.InsertItem(_T(""), hDrive);
  }

  // 展开根节点
  m_treeDrives.Expand(hRoot, TVE_EXPAND);
}

void RemoteControlViewDlg::UpdateFileList(
    const std::vector<FileSystemProtocol::FileEntry> &files) {

  // 清空列表
  m_listFiles.DeleteAllItems();

  // 插入每个文件
  int index = 0;
  for (const auto &file : files) {
    // 只显示文件,不显示目录(目录在树控件中)
    if (file.isDirectory) {
      continue;
    }

    // 插入文件名
    CString fileName(file.name.c_str());
    int nItem = m_listFiles.InsertItem(index, fileName);

    // 插入文件大小
    CString sizeStr;
    if (file.size < 1024) {
      sizeStr.Format(_T("%lld B"), file.size);
    } else if (file.size < 1024 * 1024) {
      sizeStr.Format(_T("%.2f KB"), file.size / 1024.0);
    } else if (file.size < 1024 * 1024 * 1024) {
      sizeStr.Format(_T("%.2f MB"), file.size / (1024.0 * 1024.0));
    } else {
      sizeStr.Format(_T("%.2f GB"), file.size / (1024.0 * 1024.0 * 1024.0));
    }
    m_listFiles.SetItemText(nItem, 1, sizeStr);

    // 插入文件类型(简化版,根据扩展名判断)
    CString ext = fileName.Mid(fileName.ReverseFind('.') + 1);
    m_listFiles.SetItemText(nItem, 2, ext.IsEmpty() ? _T("文件") : ext);

    // 插入修改时间(TODO: 需要FileEntry包含时间戳)
    m_listFiles.SetItemText(nItem, 3, _T("--"));

    index++;
  }
}

void RemoteControlViewDlg::UpdateSubDirectories(
    HTREEITEM hParent,
    const std::vector<FileSystemProtocol::FileEntry> &entries) {
  if (hParent == NULL) {
    return; // 没有有效的父节点，直接返回
  }

  // 添加调试日志：获取父节点路径以便跟踪
  CString parentPath = GetTreeItemPath(hParent);
  TRACE(_T("UpdateSubDirectories for parent: %s, entries count: %d\n"),
        (LPCTSTR)parentPath, (int)entries.size());

  // 删除父节点的所有子项（包括占位符）
  HTREEITEM hChild = m_treeDrives.GetChildItem(hParent);
  while (hChild != NULL) {
    HTREEITEM hNext = m_treeDrives.GetNextSiblingItem(hChild);
    // 释放存储的路径字符串
    DWORD_PTR data = m_treeDrives.GetItemData(hChild);
    if (data != 0) {
      delete reinterpret_cast<CString *>(data);
    }
    m_treeDrives.DeleteItem(hChild);
    hChild = hNext;
  }

  // 只添加目录类型的条目到树
  for (const auto &entry : entries) {
    if (!entry.isDirectory) {
      continue; // 跳过文件，只添加文件夹
    }

    // 插入子目录 - 只显示文件夹名称
    CString dirName(entry.name.c_str());
    HTREEITEM hSubDir = m_treeDrives.InsertItem(dirName, hParent);

    // 修复：验证并标准化路径格式
    // 确保使用 Windows 路径分隔符（反斜杠）
    std::string fullPath = entry.fullPath;
    std::replace(fullPath.begin(), fullPath.end(), '/', '\\');

    // 添加调试日志
    TRACE(_T("  Adding subdirectory: %s -> %S\n"), (LPCTSTR)dirName,
          fullPath.c_str());

    // 存储完整路径到节点数据中
    CString *pFullPath = new CString(fullPath.c_str());
    m_treeDrives.SetItemData(hSubDir, reinterpret_cast<DWORD_PTR>(pFullPath));

    // 为每个子目录也添加一个虚拟子节点(占位符)
    // 这样用户可以继续展开子目录
    m_treeDrives.InsertItem(_T(""), hSubDir);
  }
}

void RemoteControlViewDlg::ShowError(const std::string &message) {
  CString msg(message.c_str());
  MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
}

void RemoteControlViewDlg::ShowInfo(const std::string &message) {
  CString msg(message.c_str());
  MessageBox(msg, _T("信息"), MB_OK | MB_ICONINFORMATION);
}

void RemoteControlViewDlg::UpdateStatusBar(const std::string &text, int pane) {
  if (m_statusBar.GetSafeHwnd()) {
    CString str(text.c_str());
    m_statusBar.SetPaneText(pane, str);
  }
}

void RemoteControlViewDlg::UpdateTransferProgress(int percent,
                                                  const std::string &filename) {
  CString text;
  text.Format(_T("传输中: %s (%d%%)"), (LPCTSTR)CString(filename.c_str()),
              percent);
  UpdateStatusBar(std::string(CT2A(text)), 0);
}

// ============================================================================
// 辅助方法
// ============================================================================

void RemoteControlViewDlg::UpdateButtonStates() {
  // 文件操作按钮只有在连接时才启用
  if (m_btnRefresh.GetSafeHwnd())
    m_btnRefresh.EnableWindow(m_bConnected);
  if (m_btnUpload.GetSafeHwnd())
    m_btnUpload.EnableWindow(m_bConnected);
  if (m_btnDownload.GetSafeHwnd())
    m_btnDownload.EnableWindow(m_bConnected && !m_strSelectedFile.IsEmpty());
  if (m_btnDelete.GetSafeHwnd())
    m_btnDelete.EnableWindow(m_bConnected && !m_strSelectedFile.IsEmpty());
  if (m_btnRun.GetSafeHwnd())
    m_btnRun.EnableWindow(m_bConnected && !m_strSelectedFile.IsEmpty());

  // 监视按钮只有在连接时才启用
  if (m_btnStartMonitor.GetSafeHwnd())
    m_btnStartMonitor.EnableWindow(m_bConnected);
  if (m_btnStartRecord.GetSafeHwnd())
    m_btnStartRecord.EnableWindow(m_bConnected);

  // 工具栏按钮
  if (m_toolbar.GetSafeHwnd()) {
    m_toolbar.GetToolBarCtrl().EnableButton(ID_MVC_TOOLBAR_CONNECT,
                                            !m_bConnected);
    m_toolbar.GetToolBarCtrl().EnableButton(ID_MVC_TOOLBAR_DISCONNECT,
                                            m_bConnected);
    m_toolbar.GetToolBarCtrl().EnableButton(ID_MVC_TOOLBAR_REFRESH,
                                            m_bConnected);
  }
}

CString RemoteControlViewDlg::GetTreeItemPath(HTREEITEM hItem) {
  if (hItem == NULL)
    return _T("");

  // 统一从节点数据中获取存储的完整路径
  // 修复后，所有节点（包括驱动器节点）都使用 ItemData 存储路径
  DWORD_PTR data = m_treeDrives.GetItemData(hItem);
  if (data != 0) {
    CString *pPath = reinterpret_cast<CString *>(data);
    return *pPath;
  }

  // 如果没有路径数据，返回空（只有根节点"驱动器"没有路径数据）
  // 根节点不应该被用于路径操作，所以返回空是安全的
  return _T("");
}

void RemoteControlViewDlg::DrawStatusLED(CDC *pDC, CRect rect, bool connected) {
  // 绘制圆形指示灯
  CBrush brush(connected ? RGB(0, 255, 0) : RGB(255, 0, 0));
  CBrush *pOldBrush = pDC->SelectObject(&brush);
  pDC->Ellipse(rect);
  pDC->SelectObject(pOldBrush);
}

// ============================================================================
// 消息处理函数(将在下一部分继续实现)
// ============================================================================

void RemoteControlViewDlg::OnSysCommand(UINT nID, LPARAM lParam) {
  CDialogEx::OnSysCommand(nID, lParam);
}

void RemoteControlViewDlg::OnPaint() {
  if (IsIconic()) {
    CPaintDC dc(this);
    SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()),
                0);

    int cxIcon = GetSystemMetrics(SM_CXICON);
    int cyIcon = GetSystemMetrics(SM_CYICON);
    CRect rect;
    GetClientRect(&rect);
    int x = (rect.Width() - cxIcon + 1) / 2;
    int y = (rect.Height() - cyIcon + 1) / 2;

    dc.DrawIcon(x, y, m_hIcon);

    // 绘制状态指示灯 (通过 GetDlgItem 安全访问，避免 DDX_Control
    // 子类化断言)
    CWnd *pLed = GetDlgItem(IDC_MVC_LED_STATUS);
    if (pLed && pLed->GetSafeHwnd()) {
      CRect ledRect;
      pLed->GetWindowRect(&ledRect);
      ScreenToClient(&ledRect);
      DrawStatusLED(&dc, ledRect, m_bConnected);
    }
  } else {
    CDialogEx::OnPaint();
  }
}

HCURSOR RemoteControlViewDlg::OnQueryDragIcon() {
  return static_cast<HCURSOR>(m_hIcon);
}

void RemoteControlViewDlg::OnSize(UINT nType, int cx, int cy) {
  CDialogEx::OnSize(nType, cx, cy);

  // 重新定位工具栏和状态栏
  if (m_toolbar.GetSafeHwnd() && m_statusBar.GetSafeHwnd()) {
    RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
  }
}

// 工具栏按钮处理(委托给对应的按钮处理函数)
void RemoteControlViewDlg::OnToolbarConnect() { OnBnClickedConnect(); }
void RemoteControlViewDlg::OnToolbarDisconnect() { OnBnClickedConnect(); }
void RemoteControlViewDlg::OnToolbarRefresh() { OnBnClickedRefresh(); }
void RemoteControlViewDlg::OnToolbarSettings() { ShowInfo("设置功能待实现"); }
void RemoteControlViewDlg::OnToolbarHelp() { ShowInfo("帮助功能待实现"); }
void RemoteControlViewDlg::OnToolbarAbout() {
  ShowInfo("RemoteControl MVC版本 v1.0");
}

// 连接按钮
void RemoteControlViewDlg::OnBnClickedConnect() {
  if (!controller_)
    return;

  if (!m_bConnected) {
    // 获取IP
    std::string ip = "127.0.0.1";
    if (m_ipAddress.GetSafeHwnd()) {
      BYTE b1, b2, b3, b4;
      m_ipAddress.GetAddress(b1, b2, b3, b4);
      std::ostringstream oss;
      oss << (int)b1 << "." << (int)b2 << "." << (int)b3 << "." << (int)b4;
      ip = oss.str();
    }

    // 获取端口
    uint16_t port = 9527;
    if (m_editPort.GetSafeHwnd()) {
      CString portStr;
      m_editPort.GetWindowText(portStr);
      if (!portStr.IsEmpty()) {
        port = (uint16_t)_ttoi(portStr);
      }
    }

    // 委托给Controller
    controller_->OnConnectRequested(ip, port);
  } else {
    // 断开连接
    controller_->OnDisconnectRequested();
  }
}

void RemoteControlViewDlg::OnIpAddressChanged(NMHDR *pNMHDR, LRESULT *pResult) {
  *pResult = 0;
}

void RemoteControlViewDlg::OnPortChanged() {}

// 文件浏览
void RemoteControlViewDlg::OnBnClickedRefresh() {
  if (!controller_)
    return;

  // 如果有选中的目录,则刷新该目录
  HTREEITEM hItem = m_treeDrives.GetSelectedItem();
  if (hItem != NULL) {
    CString path = GetTreeItemPath(hItem);
    if (!path.IsEmpty() && path != _T("驱动器")) {
      // 清除防抖缓存,强制重新加载
      m_strLastSelectedPath = _T("");

      // 重新加载当前目录的文件列表
      controller_->OnDirectorySelected(std::string(CT2A(path)));

      // 如果节点已展开,也刷新子目录
      if (m_treeDrives.GetItemState(hItem, TVIS_EXPANDED) & TVIS_EXPANDED) {
        // 删除所有子节点,添加占位符,以便重新加载
        HTREEITEM hChild = m_treeDrives.GetChildItem(hItem);
        while (hChild != NULL) {
          HTREEITEM hNext = m_treeDrives.GetNextSiblingItem(hChild);
          DWORD_PTR data = m_treeDrives.GetItemData(hChild);
          if (data != 0) {
            delete reinterpret_cast<CString *>(data);
          }
          m_treeDrives.DeleteItem(hChild);
          hChild = hNext;
        }
        // 添加占位符
        m_treeDrives.InsertItem(_T(""), hItem);

        // 触发重新加载（路径信息会随消息一起传递）
        controller_->OnDirectoryExpanded(std::string(CT2A(path)));
      }
      return;
    }
  }

  // 否则刷新驱动器列表
  controller_->OnRefreshDrives();
}

void RemoteControlViewDlg::OnTreeSelChanged(NMHDR *pNMHDR, LRESULT *pResult) {
  LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
  *pResult = 0;

  if (!controller_)
    return;

  HTREEITEM hItem = pNMTreeView->itemNew.hItem;
  if (hItem == NULL)
    return;

  CString path = GetTreeItemPath(hItem);
  if (path.IsEmpty() || path == _T("驱动器")) {
    m_listFiles.DeleteAllItems();
    return;
  }

  // 更新当前路径
  m_strCurrentPath = path;
  m_strLastSelectedPath = path;
  m_editPath.SetWindowText(path);

  // 注意：不设置 m_hExpandingItem，因为选择节点不应该更新子目录结构
  // 子目录结构只在展开节点时更新（OnTreeItemExpanding）

  // 委托给Controller加载文件列表（只更新右侧列表，不更新左侧树）
  controller_->OnDirectorySelected(std::string(CT2A(path)));
}

void RemoteControlViewDlg::OnTreeDblClick(NMHDR *pNMHDR, LRESULT *pResult) {
  *pResult = 0;

  if (!controller_)
    return;

  HTREEITEM hItem = m_treeDrives.GetSelectedItem();
  if (hItem == NULL)
    return;

  CString path = GetTreeItemPath(hItem);
  if (path.IsEmpty() || path == _T("驱动器")) {
    OnBnClickedRefresh();
    return;
  }

  // 委托给Controller展开目录
  controller_->OnDirectoryExpanded(std::string(CT2A(path)));
}

void RemoteControlViewDlg::OnTreeItemExpanding(NMHDR *pNMHDR,
                                               LRESULT *pResult) {
  LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
  *pResult = 0;

  // 只处理展开事件，不处理折叠
  if (pNMTreeView->action != TVE_EXPAND)
    return;

  HTREEITEM hItem = pNMTreeView->itemNew.hItem;
  if (hItem == NULL)
    return;

  // 获取节点路径
  CString path = GetTreeItemPath(hItem);
  if (path.IsEmpty() || path == _T("驱动器")) {
    return;
  }

  // 添加调试日志以追踪展开操作
  TRACE(_T("OnTreeItemExpanding: %s\n"), (LPCTSTR)path);

  // 检查是否只有占位符子节点(空文本的子节点)
  HTREEITEM hChild = m_treeDrives.GetChildItem(hItem);
  bool hasPlaceholder = false;
  if (hChild != NULL) {
    CString childText = m_treeDrives.GetItemText(hChild);
    if (childText.IsEmpty()) {
      hasPlaceholder = true;
    }
  }

  // 只有当存在占位符时才加载(首次展开)
  // 如果已经有实际内容,说明已加载过,不重复加载
  if (!hasPlaceholder && hChild != NULL) {
    TRACE(_T("  Already loaded, skipping\n"));
    return;
  }

  // 验证 fileSystem_ Model 是否有效
  if (!fileSystem_) {
    TRACE(_T("  Error: fileSystem_ is null\n"));
    return;
  }

  // 显式使用 CT2A 转换对象以避免编译器将 CT2A 解析为函数重载
  CT2A convPath(path);
  std::string pathStr(static_cast<const char *>(convPath));
  HWND hWnd = GetSafeHwnd();

  // 异步加载子目录，在回调中使用 PostMessage 更新 UI
  fileSystem_->listDirectory(
      pathStr, [hWnd, hItem, pathStr](
                   const std::vector<FileSystemProtocol::FileEntry> &entries,
                   bool hasMore) {
        // 验证窗口是否仍然有效
        if (!::IsWindow(hWnd)) {
          return;
        }

        // 创建数据结构，包含 HTREEITEM 句柄
        // 注意：hItem 可能在回调执行时已失效，我们将在 OnUpdateSubDirs 中验证
        auto *pData = new SubDirUpdateData();
        pData->hParent = hItem;
        pData->entries = entries;
        ::PostMessage(hWnd, WM_UPDATE_SUB_DIRS, (WPARAM)pData, 0);
      });
}

void RemoteControlViewDlg::OnListRightClick(NMHDR *pNMHDR, LRESULT *pResult) {
  LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
  *pResult = 0;

  // 获取选中项
  POSITION pos = m_listFiles.GetFirstSelectedItemPosition();
  if (pos == NULL)
    return;

  int nItem = m_listFiles.GetNextSelectedItem(pos);
  m_strSelectedFile = m_listFiles.GetItemText(nItem, 0);

  // 更新按钮状态
  UpdateButtonStates();

  // 显示右键菜单
  CMenu menu;
  menu.CreatePopupMenu();
  menu.AppendMenu(MF_STRING, ID_MVC_MENU_DOWNLOAD, _T("下载文件"));
  menu.AppendMenu(MF_STRING, ID_MVC_MENU_DELETE, _T("删除文件"));
  menu.AppendMenu(MF_STRING, ID_MVC_MENU_RUN, _T("运行文件"));
  menu.AppendMenu(MF_SEPARATOR);
  menu.AppendMenu(MF_STRING, ID_MVC_MENU_PROPERTIES, _T("属性"));

  CPoint point;
  GetCursorPos(&point);
  menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

void RemoteControlViewDlg::OnListDblClick(NMHDR *pNMHDR, LRESULT *pResult) {
  *pResult = 0;
  // 双击文件默认下载
  OnMenuDownload();
}

// 文件操作按钮
void RemoteControlViewDlg::OnBnClickedUpload() {
  if (!controller_)
    return;

  CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
                  _T("所有文件 (*.*)|*.*||"), this);

  if (dlg.DoModal() == IDOK) {
    CString localPath = dlg.GetPathName();
    CString remotePath = m_strCurrentPath + _T("\\") + dlg.GetFileName();

    controller_->OnFileUpload(std::string(CT2A(localPath)),
                              std::string(CT2A(remotePath)));
  }
}

void RemoteControlViewDlg::OnBnClickedDownload() { OnMenuDownload(); }

void RemoteControlViewDlg::OnBnClickedDelete() { OnMenuDelete(); }

void RemoteControlViewDlg::OnBnClickedRun() { OnMenuRun(); }

// 右键菜单
void RemoteControlViewDlg::OnMenuDownload() {
  if (!controller_ || m_strSelectedFile.IsEmpty())
    return;

  CFileDialog dlg(FALSE, NULL, m_strSelectedFile,
                  OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                  _T("所有文件 (*.*)|*.*||"), this);

  if (dlg.DoModal() == IDOK) {
    CString remotePath = m_strCurrentPath + _T("\\") + m_strSelectedFile;
    CString localPath = dlg.GetPathName();

    controller_->OnFileDownload(std::string(CT2A(remotePath)),
                                std::string(CT2A(localPath)));
  }
}

void RemoteControlViewDlg::OnMenuUpload() { OnBnClickedUpload(); }

void RemoteControlViewDlg::OnMenuDelete() {
  if (!controller_ || m_strSelectedFile.IsEmpty())
    return;

  CString msg;
  msg.Format(_T("确定要删除文件 '%s' 吗?"), (LPCTSTR)m_strSelectedFile);

  if (MessageBox(msg, _T("确认删除"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
    CString remotePath = m_strCurrentPath + _T("\\") + m_strSelectedFile;
    controller_->OnFileDelete(std::string(CT2A(remotePath)));
  }
}

void RemoteControlViewDlg::OnMenuRun() {
  if (!controller_ || m_strSelectedFile.IsEmpty())
    return;

  CString remotePath = m_strCurrentPath + _T("\\") + m_strSelectedFile;
  controller_->OnFileRun(std::string(CT2A(remotePath)));
}

void RemoteControlViewDlg::OnMenuProperties() { ShowInfo("属性功能待实现"); }

// 监视功能
void RemoteControlViewDlg::OnBnClickedStartMonitor() {
  if (!controller_)
    return;
  controller_->OnStartMonitor();
}

void RemoteControlViewDlg::OnBnClickedStartRecord() {
  ShowInfo("录制功能待实现");
}
