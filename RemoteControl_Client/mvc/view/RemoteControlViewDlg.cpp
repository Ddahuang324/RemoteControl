#include "pch.h"
#include "RemoteControlViewDlg.h"
#include "afxdialogex.h"

#include <algorithm>
#include <sstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ============================================================================
// ============================================================================
// 鏋勯€犱笌鏋愭瀯
// ============================================================================

RemoteControlViewDlg::RemoteControlViewDlg(CWnd *pParent)
  : CDialogEx(IDD_MVC_MAIN_DIALOG, pParent), controller_(nullptr) {
  // 初始化 ViewProtocol 容器并迁移旧状态
  m_protocol = std::make_unique<ViewProtocol::MainViewProtocol>();
  m_protocol->state->isConnected = false;
  m_protocol->state->isMonitoring = false;
  m_protocol->state->isFileListCleared = true;
  m_protocol->state->currentPath = _T("");
  m_protocol->state->selectedFile = _T("");
  m_protocol->state->lastSelectedPath = _T("");
  m_protocol->sysRes->hIcon = nullptr;
}

RemoteControlViewDlg::~RemoteControlViewDlg() {
  // 閲婃斁鎵€鏈夋爲鑺傜偣涓瓨鍌ㄧ殑璺緞鏁版嵁
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

  // ===== 涓存椂绠€鍖栫増鏈?- 鐢ㄤ簬鎺掓煡闂 =====
  // 鍙粦瀹氭渶鍩烘湰鐨勬帶浠?閫愭娴嬭瘯

  // 娴嬭瘯 1: 鍙粦瀹氭寜閽?
  // 缃戠粶杩炴帴鍖哄煙
  DDX_Control(pDX, IDC_MVC_BTN_CONNECT, m_btnConnect);

  DDX_Control(pDX, IDC_MVC_STATIC_IP, m_staticIP);
  DDX_Control(pDX, IDC_MVC_IPADDRESS, m_ipAddress);
  DDX_Control(pDX, IDC_MVC_STATIC_PORT, m_staticPort);
  DDX_Control(pDX, IDC_MVC_EDIT_PORT, m_editPort);
  DDX_Control(pDX, IDC_MVC_STATIC_STATUS, m_staticStatus);

  // 鏂囦欢绠＄悊鍖哄煙
  DDX_Control(pDX, IDC_MVC_TREE_DRIVES, m_treeDrives);
  DDX_Control(pDX, IDC_MVC_LIST_FILES, m_listFiles);
  DDX_Control(pDX, IDC_MVC_EDIT_PATH, m_editPath);
  DDX_Control(pDX, IDC_MVC_BTN_REFRESH, m_btnRefresh);
  DDX_Control(pDX, IDC_MVC_BTN_UPLOAD, m_btnUpload);
  DDX_Control(pDX, IDC_MVC_BTN_DOWNLOAD, m_btnDownload);
  DDX_Control(pDX, IDC_MVC_BTN_DELETE, m_btnDelete);
  DDX_Control(pDX, IDC_MVC_BTN_RUN, m_btnRun);

  // 鐩戣鍖哄煙
  DDX_Control(pDX, IDC_MVC_BTN_START_MONITOR, m_btnStartMonitor);
  DDX_Control(pDX, IDC_MVC_BTN_START_RECORD, m_btnStartRecord);

  // 鍒嗙粍妗?
  DDX_Control(pDX, IDC_MVC_GROUP_CONNECTION, m_groupConnection);
  DDX_Control(pDX, IDC_MVC_GROUP_FILES, m_groupFiles);
  DDX_Control(pDX, IDC_MVC_GROUP_MONITOR, m_groupMonitor);
}

BEGIN_MESSAGE_MAP(RemoteControlViewDlg, CDialogEx)
ON_WM_SYSCOMMAND()
ON_WM_PAINT()
ON_WM_QUERYDRAGICON()
ON_WM_SIZE()
ON_WM_GETMINMAXINFO()
ON_MESSAGE(WM_APP + 0x100, &RemoteControlViewDlg::OnNetworkStatusChanged)
ON_MESSAGE(WM_UPDATE_DRIVE_LIST, &RemoteControlViewDlg::OnUpdateDriveList)
ON_MESSAGE(WM_UPDATE_FILE_LIST, &RemoteControlViewDlg::OnUpdateFileList)
ON_MESSAGE(WM_UPDATE_SUB_DIRS, &RemoteControlViewDlg::OnUpdateSubDirs)

// 宸ュ叿鏍?
ON_COMMAND(ID_MVC_TOOLBAR_CONNECT, &RemoteControlViewDlg::OnToolbarConnect)
ON_COMMAND(ID_MVC_TOOLBAR_DISCONNECT,
           &RemoteControlViewDlg::OnToolbarDisconnect)
ON_COMMAND(ID_MVC_TOOLBAR_REFRESH, &RemoteControlViewDlg::OnToolbarRefresh)
ON_COMMAND(ID_MVC_TOOLBAR_SETTINGS, &RemoteControlViewDlg::OnToolbarSettings)
ON_COMMAND(ID_MVC_TOOLBAR_HELP, &RemoteControlViewDlg::OnToolbarHelp)
ON_COMMAND(ID_MVC_TOOLBAR_ABOUT, &RemoteControlViewDlg::OnToolbarAbout)

// 杩炴帴鎸夐挳
ON_BN_CLICKED(IDC_MVC_BTN_CONNECT, &RemoteControlViewDlg::OnBnClickedConnect)
ON_NOTIFY(IPN_FIELDCHANGED, IDC_MVC_IPADDRESS,
          &RemoteControlViewDlg::OnIpAddressChanged)
ON_EN_CHANGE(IDC_MVC_EDIT_PORT, &RemoteControlViewDlg::OnPortChanged)

// 鏂囦欢娴忚
ON_BN_CLICKED(IDC_MVC_BTN_REFRESH, &RemoteControlViewDlg::OnBnClickedRefresh)
ON_NOTIFY(TVN_SELCHANGED, IDC_MVC_TREE_DRIVES,
          &RemoteControlViewDlg::OnTreeSelChanged)
ON_NOTIFY(TVN_ITEMEXPANDING, IDC_MVC_TREE_DRIVES,
          &RemoteControlViewDlg::OnTreeItemExpanding)
ON_NOTIFY(NM_DBLCLK, IDC_MVC_TREE_DRIVES, &RemoteControlViewDlg::OnTreeDblClick)
ON_NOTIFY(NM_RCLICK, IDC_MVC_LIST_FILES,
          &RemoteControlViewDlg::OnListRightClick)
ON_NOTIFY(NM_DBLCLK, IDC_MVC_LIST_FILES, &RemoteControlViewDlg::OnListDblClick)

// 鏂囦欢鎿嶄綔鎸夐挳
ON_BN_CLICKED(IDC_MVC_BTN_UPLOAD, &RemoteControlViewDlg::OnBnClickedUpload)
ON_BN_CLICKED(IDC_MVC_BTN_DOWNLOAD, &RemoteControlViewDlg::OnBnClickedDownload)
ON_BN_CLICKED(IDC_MVC_BTN_DELETE, &RemoteControlViewDlg::OnBnClickedDelete)
ON_BN_CLICKED(IDC_MVC_BTN_RUN, &RemoteControlViewDlg::OnBnClickedRun)

// 鍙抽敭鑿滃崟
ON_COMMAND(ID_MVC_MENU_DOWNLOAD, &RemoteControlViewDlg::OnMenuDownload)
ON_COMMAND(ID_MVC_MENU_UPLOAD, &RemoteControlViewDlg::OnMenuUpload)
ON_COMMAND(ID_MVC_MENU_DELETE, &RemoteControlViewDlg::OnMenuDelete)
ON_COMMAND(ID_MVC_MENU_RUN, &RemoteControlViewDlg::OnMenuRun)
ON_COMMAND(ID_MVC_MENU_PROPERTIES, &RemoteControlViewDlg::OnMenuProperties)

// 鐩戣鍔熻兘
ON_BN_CLICKED(IDC_MVC_BTN_START_MONITOR,
              &RemoteControlViewDlg::OnBnClickedStartMonitor)
ON_BN_CLICKED(IDC_MVC_BTN_START_RECORD,
              &RemoteControlViewDlg::OnBnClickedStartRecord)
END_MESSAGE_MAP()

// ============================================================================
// 鍒濆鍖?
// ============================================================================

BOOL RemoteControlViewDlg::OnInitDialog() {
  CDialogEx::OnInitDialog();

  // 璁剧疆鍥炬爣
  // TODO: 鍔犺浇瀹為檯鐨勫簲鐢ㄧ▼搴忓浘鏍?
  // SetIcon(m_hIcon, TRUE);
  // SetIcon(m_hIcon, FALSE);

  // 鍒濆鍖栧伐鍏锋爮
  InitToolbar();

  // 鍒濆鍖栫姸鎬佹爮
  InitStatusBar();

  // 鍒濆鍖栨枃浠跺垪琛?
  InitFileList();

  // 璁剧疆榛樿IP鍜岀鍙?鍦ㄦ帶浠舵垚鍔熷垱寤哄悗鍐嶅啓鍏?閬垮厤璋冭瘯鐗堟柇瑷€)
  if (m_ipAddress.GetSafeHwnd()) {
    m_ipAddress.SetAddress(127, 0, 0, 1);
  } else {
    TRACE0("Warning: IP address control is not available, skip default "
           "address.\n");
  }
  if (m_editPort.GetSafeHwnd()) {
    m_editPort.SetWindowText(_T("9527"));
  }

  // 鍒濆鍖栨寜閽姸鎬?
  UpdateButtonStates();

  // Set initial status bar text
  // TODO: restore localized string if needed
  UpdateStatusBar("Ready", 0);

  // ===== 响应式布局初始化 =====
  // 获取对话框初始大小并居中显示
  int screenW = GetSystemMetrics(SM_CXSCREEN);
  int screenH = GetSystemMetrics(SM_CYSCREEN);
  CRect rcWindow;
  GetWindowRect(&rcWindow);
  int windowW = rcWindow.Width();
  int windowH = rcWindow.Height();
  // 将窗口居中于屏幕
  MoveWindow((screenW - windowW) / 2, (screenH - windowH) / 2, windowW, windowH,
             FALSE);

  // 记录初始客户区大小到协议容器
  if (m_protocol && m_protocol->layout) {
    GetClientRect(&m_protocol->layout->originalDialogRect);

    // 定义布局规则（锚点） - 存入协议
    m_protocol->layout->rules.clear();
    m_protocol->layout->rules.push_back(
        {IDC_MVC_GROUP_CONNECTION, true, true, true, false});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_STATIC_IP, true, true, false, false});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_IPADDRESS, true, true, false, false});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_STATIC_PORT, true, true, false, false});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_EDIT_PORT, true, true, false, false});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_BTN_CONNECT, true, true, false, false});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_STATIC_STATUS, true, true, false, false});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_LED_STATUS, true, true, false, false});

    m_protocol->layout->rules.push_back(
        {IDC_MVC_GROUP_FILES, true, true, true, true});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_TREE_DRIVES, true, true, false, true});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_EDIT_PATH, true, true, true, false});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_LIST_FILES, true, true, true, true});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_BTN_REFRESH, true, false, false, true});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_BTN_UPLOAD, true, false, false, true});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_BTN_DOWNLOAD, true, false, false, true});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_BTN_DELETE, true, false, false, true});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_BTN_RUN, true, false, false, true});

    m_protocol->layout->rules.push_back(
        {IDC_MVC_GROUP_MONITOR, true, false, true, true});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_BTN_START_MONITOR, true, false, false, true});
    m_protocol->layout->rules.push_back(
        {IDC_MVC_BTN_START_RECORD, true, false, false, true});

    // 记录所有受控件的初始 Rect
    m_protocol->layout->originalRects.clear();
    for (const auto &rule : m_protocol->layout->rules) {
      CWnd *pWnd = GetDlgItem(rule.nID);
      if (pWnd && pWnd->GetSafeHwnd()) {
        CRect r;
        pWnd->GetWindowRect(&r);
        ScreenToClient(&r);
        m_protocol->layout->originalRects[rule.nID] = r;
      }
    }
  }

  return TRUE;
}

// 鎺ユ敹鏉ヨ嚜鍚庡彴绾跨▼鐨勭綉缁滅姸鎬侀€氱煡 (wParam: connected)
LRESULT RemoteControlViewDlg::OnNetworkStatusChanged(WPARAM wParam,
                                                     LPARAM lParam) {
  bool connected = (wParam != 0);
  UpdateConnectionStatus(connected);
  if (connected) {
    ShowInfo("连接成功！");
    UpdateStatusBar("已连接", 0);
  } else {
    ShowError("连接失败，请检查 IP 和端口");
    UpdateStatusBar("连接失败", 0);
  }
  return 0;
}

// 鎺ユ敹鏉ヨ嚜鍚庡彴绾跨▼鐨勯┍鍔ㄥ櫒鍒楄〃鏇存柊 (wParam: 鏁版嵁鎸囬拡)
LRESULT RemoteControlViewDlg::OnUpdateDriveList(WPARAM wParam, LPARAM lParam) {
  auto *pDrives = reinterpret_cast<std::vector<std::string> *>(wParam);
  if (pDrives) {
    UpdateDriveList(*pDrives);
    UpdateStatusBar("驱动器已刷新", 0);
    delete pDrives; // 閲婃斁鍐呭瓨
  }
  return 0;
}

// 鎺ユ敹鏉ヨ嚜鍚庡彴绾跨▼鐨勬枃浠跺垪琛ㄦ洿鏂?(wParam: 鏁版嵁鎸囬拡, lParam:
// hasMore)
LRESULT RemoteControlViewDlg::OnUpdateFileList(WPARAM wParam, LPARAM lParam) {
  // 如果 wParam 为 0，表示这是目录切换的开始，需要重置清空标记
  // 我们不立即清空列表，而是等到第一批数据到达时再清空，以避免闪烁
  if (wParam == 0) {
    if (m_protocol && m_protocol->state) {
      m_protocol->state->isFileListCleared = false;
    }
    return 0;
  }

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

// 鎺ユ敹鏉ヨ嚜鍚庡彴绾跨▼鐨勫瓙鐩綍鏇存柊 (wParam:
// SubDirUpdateData鎸囬拡)
LRESULT RemoteControlViewDlg::OnUpdateSubDirs(WPARAM wParam, LPARAM lParam) {
  auto *pData = reinterpret_cast<SubDirUpdateData *>(wParam);

  if (pData) {
    // 楠岃瘉鏍戣妭鐐瑰彞鏌勬槸鍚︿粛鐒舵湁鏁?
    // 鍦ㄥ紓姝ュ洖璋冩湡闂达紝鐢ㄦ埛鍙兘宸茬粡鍒犻櫎鎴栦慨鏀逛簡鏍戣妭鐐?
    if (pData->hParent != NULL) {
      // 灏濊瘯鑾峰彇鑺傜偣璺緞锛屽鏋滃け璐ヨ鏄庤妭鐐瑰凡琚垹闄?
      CString testPath = GetTreeItemPath(pData->hParent);
      if (!testPath.IsEmpty()) {
        // 鑺傜偣浠嶇劧鏈夋晥锛屾洿鏂板瓙鐩綍
        TRACE(
            _T("OnUpdateSubDirs: hParent is valid, updating subdirectories\n"));
        UpdateSubDirectories(pData->hParent, pData->entries, pData->isFirst);
        UpdateStatusBar("选中节点已添加", 0);
      } else {
        // 鑺傜偣宸插け鏁堬紝璁板綍鏃ュ織浣嗕笉宕╂簝
        TRACE(_T("OnUpdateSubDirs: hParent is invalid (node deleted), ")
              _T("skipping update\n"));
      }
    } else {
      TRACE(_T("OnUpdateSubDirs: hParent is NULL\n"));
    }
    delete pData; // 閲婃斁鍐呭瓨
  }
  return 0;
}

void RemoteControlViewDlg::InitToolbar() {
  // 鍒涘缓宸ュ叿鏍?
  DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS |
                  CBRS_FLYBY | CBRS_SIZE_DYNAMIC;
  // 先采用传统扁平图标样式，避免文字占用过多高度
  if (!m_toolbar.CreateEx(this, TBSTYLE_FLAT, dwStyle)) {
    TRACE0("Failed to create toolbar\n");
    return;
  }

  // 璁剧疆宸ュ叿鏍忔寜閽?绠€鍖栫増,瀹為檯搴斿姞杞戒綅鍥?
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

  // 先只用小图标按钮，文字通过 ToolTip/状态栏提示
  CSize imgSize(16, 16);
  CSize btnSize(24, 24);
  m_toolbar.GetToolBarCtrl().SetBitmapSize(imgSize);
  m_toolbar.GetToolBarCtrl().SetButtonSize(btnSize);
  m_toolbar.SetSizes(btnSize, imgSize);

  // 閲嶆柊璁＄畻甯冨眬
  RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
}

void RemoteControlViewDlg::InitStatusBar() {
  // 鍒涘缓鐘舵€佹爮
  if (!m_statusBar.Create(this)) {
    TRACE0("Failed to create status bar\n");
    return;
  }

  // 璁剧疆鐘舵€佹爮鍒嗘爮
  static UINT indicators[] = {
      ID_SEPARATOR, // 鐘舵€佹枃鏈?
      ID_SEPARATOR, // 浼犺緭閫熷害
      ID_SEPARATOR, // 缃戠粶寤惰繜
  };

  m_statusBar.SetIndicators(indicators, _countof(indicators));
  m_statusBar.SetPaneInfo(0, ID_SEPARATOR, SBPS_STRETCH, 0);
  m_statusBar.SetPaneInfo(1, ID_SEPARATOR, SBPS_NORMAL, 150);
  m_statusBar.SetPaneInfo(2, ID_SEPARATOR, SBPS_NORMAL, 100);

  m_statusBar.SetPaneText(0, _T("就绪"));
  m_statusBar.SetPaneText(1, _T("传输: 0 KB/s"));
  m_statusBar.SetPaneText(2, _T("延迟: 0 ms"));

  // 閲嶆柊璁＄畻甯冨眬
  RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
}

void RemoteControlViewDlg::InitFileList() {
  // 璁剧疆鍒楄〃鎺т欢涓鸿缁嗕俊鎭ā寮?
  m_listFiles.ModifyStyle(0, LVS_REPORT);
  m_listFiles.SetExtendedStyle(m_listFiles.GetExtendedStyle() |
                               LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  // 娣诲姞鍒?
  m_listFiles.InsertColumn(0, _T("名称"), LVCFMT_LEFT, 200);
  m_listFiles.InsertColumn(1, _T("大小"), LVCFMT_RIGHT, 100);
  m_listFiles.InsertColumn(2, _T("类型"), LVCFMT_LEFT, 100);
  m_listFiles.InsertColumn(3, _T("修改时间"), LVCFMT_LEFT, 150);
}

// ============================================================================
// Controller娉ㄥ叆
// ============================================================================

void RemoteControlViewDlg::SetController(
    std::shared_ptr<IMainController> controller) {
  controller_ = controller;
}

// ============================================================================
// View鎺ュ彛瀹炵幇: 渚汣ontroller璋冪敤浠ユ洿鏂癠I
// ============================================================================

void RemoteControlViewDlg::UpdateConnectionStatus(bool connected) {
  if (m_protocol && m_protocol->state)
    m_protocol->state->isConnected = connected;

  // 鏇存柊鎸夐挳鏂囨湰
  if (m_btnConnect.GetSafeHwnd()) {
    m_btnConnect.SetWindowText(connected ? _T("断开连接") : _T("连接"));
  }

  // 鏇存柊鐘舵€佹枃鏈?
  if (m_staticStatus.GetSafeHwnd()) {
    m_staticStatus.SetWindowText(connected ? _T("已连接") : _T("未连接"));
  }

  // 瑙﹀彂鐘舵€佹寚绀虹伅閲嶇粯
  CWnd *pLed = GetDlgItem(IDC_MVC_LED_STATUS);
  if (pLed && pLed->GetSafeHwnd()) {
    pLed->Invalidate();
  }

  // 鏇存柊鎸夐挳鐘舵€?
  UpdateButtonStates();

  // 鏇存柊鐘舵€佹爮
  UpdateStatusBar(connected ? "已连接" : "未连接", 0);
}

void RemoteControlViewDlg::UpdateDriveList(
    const std::vector<std::string> &drives) {
  // 娓呯┖鏍戞帶浠跺墠,鍏堥噴鏀炬墍鏈夊瓨鍌ㄧ殑璺緞鏁版嵁
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

  // 娓呯┖鏍戞帶浠?
  m_treeDrives.DeleteAllItems();

  // 鎻掑叆鏍硅妭鐐? (驱动器)
  hRoot = m_treeDrives.InsertItem(_T("驱动器"));

  // 鎻掑叆姣忎釜椹卞姩鍣?
  for (const auto &drive : drives) {
    CString driveName(drive.c_str());
    HTREEITEM hDrive = m_treeDrives.InsertItem(driveName, hRoot);

    // 淇锛氫负椹卞姩鍣ㄨ妭鐐逛篃瀛樺偍瀹屾暣璺緞鍒?ItemData
    // 杩欑‘淇濇墍鏈夎妭鐐癸紙椹卞姩鍣ㄥ拰瀛愮洰褰曪級閮戒娇鐢ㄧ浉鍚岀殑璺緞瀛樺偍鏈哄埗
    CString *pDrivePath = new CString(drive.c_str());
    m_treeDrives.SetItemData(hDrive, reinterpret_cast<DWORD_PTR>(pDrivePath));

    // 涓烘瘡涓鐩樻坊鍔犱竴涓櫄鎷熷瓙鑺傜偣(鍗犱綅绗?
    // 杩欐牱鏍戞帶浠朵細鏄剧ず灞曞紑鎸夐挳锛岀敤鎴峰彲浠ョ偣鍑诲睍寮€
    m_treeDrives.InsertItem(_T(""), hDrive);
  }

  // 灞曞紑鏍硅妭鐐?
  m_treeDrives.Expand(hRoot, TVE_EXPAND);
}

void RemoteControlViewDlg::UpdateFileList(
    const std::vector<FileSystemProtocol::FileEntry> &files) {

  // 如果这是新目录的第一批数据，清空列表
  if (m_protocol && m_protocol->state &&
      !m_protocol->state->isFileListCleared) {
    m_listFiles.DeleteAllItems();
    m_protocol->state->isFileListCleared = true;
  }

  // m_listFiles.DeleteAllItems();

  // 鎻掑叆姣忎釜鏂囦欢
  int index = m_listFiles.GetItemCount();
  for (const auto &file : files) {
    // 鍙樉绀烘枃浠?涓嶆樉绀虹洰褰?鐩綍鍦ㄦ爲鎺т欢涓?
    if (file.isDirectory) {
      continue;
    }

    // 鎻掑叆鏂囦欢鍚?
    CString fileName(file.name.c_str());
    int nItem = m_listFiles.InsertItem(index, fileName);

    // 鎻掑叆鏂囦欢澶у皬
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

    // 鎻掑叆鏂囦欢绫诲瀷(绠€鍖栫増,鏍规嵁鎵╁睍鍚嶅垽鏂?
    CString ext = fileName.Mid(fileName.ReverseFind('.') + 1);
    m_listFiles.SetItemText(nItem, 2, ext.IsEmpty() ? _T("文件") : ext);

    // 鎻掑叆淇敼鏃堕棿(TODO:
    // 闇€瑕丗ileEntry鍖呭惈鏃堕棿鎴?
    m_listFiles.SetItemText(nItem, 3, _T("--"));

    index++;
  }
}

void RemoteControlViewDlg::UpdateSubDirectories(
    HTREEITEM hParent,
    const std::vector<FileSystemProtocol::FileEntry> &entries, bool isFirst) {
  if (hParent == NULL) {
    return; // 娌℃湁鏈夋晥鐨勭埗鑺傜偣锛岀洿鎺ヨ繑鍥?
  }

  // 娣诲姞璋冭瘯鏃ュ織锛氳幏鍙栫埗鑺傜偣璺緞浠ヤ究璺熻釜
  CString parentPath = GetTreeItemPath(hParent);
  TRACE(_T("UpdateSubDirectories for parent: %s, entries count: %d, isFirst: ")
        _T("%d\n"),
        (LPCTSTR)parentPath, (int)entries.size(), isFirst);

  // 仅在第一批数据时删除父节点的所有子项（包括占位符）
  if (isFirst) {
    HTREEITEM hChild = m_treeDrives.GetChildItem(hParent);
    while (hChild != NULL) {
      HTREEITEM hNext = m_treeDrives.GetNextSiblingItem(hChild);
      // 閲婃斁瀛樺偍鐨勮矾寰勫瓧绗︿覆
      DWORD_PTR data = m_treeDrives.GetItemData(hChild);
      if (data != 0) {
        delete reinterpret_cast<CString *>(data);
      }
      m_treeDrives.DeleteItem(hChild);
      hChild = hNext;
    }
  }

  // 鍙坊鍔犵洰褰曠被鍨嬬殑鏉＄洰鍒版爲
  for (const auto &entry : entries) {
    if (!entry.isDirectory) {
      continue; // 璺宠繃鏂囦欢锛屽彧娣诲姞鏂囦欢澶?
    }

    // 鎻掑叆瀛愮洰褰?- 鍙樉绀烘枃浠跺す鍚嶇О
    CString dirName(entry.name.c_str());
    HTREEITEM hSubDir = m_treeDrives.InsertItem(dirName, hParent);

    // 淇锛氶獙璇佸苟鏍囧噯鍖栬矾寰勬牸寮?
    // 纭繚浣跨敤 Windows 璺緞鍒嗛殧绗︼紙鍙嶆枩鏉狅級
    std::string fullPath = entry.fullPath;
    std::replace(fullPath.begin(), fullPath.end(), '/', '\\');

    // 娣诲姞璋冭瘯鏃ュ織
    TRACE(_T("  Adding subdirectory: %s -> %S\n"), (LPCTSTR)dirName,
          fullPath.c_str());

    // 瀛樺偍瀹屾暣璺緞鍒拌妭鐐规暟鎹腑
    CString *pFullPath = new CString(fullPath.c_str());
    m_treeDrives.SetItemData(hSubDir, reinterpret_cast<DWORD_PTR>(pFullPath));

    // 涓烘瘡涓瓙鐩綍涔熸坊鍔犱竴涓櫄鎷熷瓙鑺傜偣(鍗犱綅绗?
    // 杩欐牱鐢ㄦ埛鍙互缁х画灞曞紑瀛愮洰褰?
    m_treeDrives.InsertItem(_T(""), hSubDir);
  }
}

void RemoteControlViewDlg::ShowError(const std::string &message) {
  CString msg(message.c_str());
  MessageBox(msg, _T("閿欒"), MB_OK | MB_ICONERROR);
}

void RemoteControlViewDlg::ShowInfo(const std::string &message) {
  CString msg(message.c_str());
  MessageBox(msg, _T("淇℃伅"), MB_OK | MB_ICONINFORMATION);
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
  text.Format(_T("传输中 %s (%d%%)"), (LPCTSTR)CString(filename.c_str()),
              percent);
  UpdateStatusBar(std::string(CT2A(text)), 0);
}

// ============================================================================
// 杈呭姪鏂规硶
// ============================================================================

void RemoteControlViewDlg::UpdateButtonStates() {
  // 鏂囦欢鎿嶄綔鎸夐挳鍙湁鍦ㄨ繛鎺ユ椂鎵嶅惎鐢?
  bool connected = (m_protocol && m_protocol->state)
                       ? m_protocol->state->isConnected
                       : false;
  bool hasSelection = (m_protocol && m_protocol->state)
                          ? !m_protocol->state->selectedFile.IsEmpty()
                          : false;
  if (m_btnRefresh.GetSafeHwnd())
    m_btnRefresh.EnableWindow(connected);
  if (m_btnUpload.GetSafeHwnd())
    m_btnUpload.EnableWindow(connected);
  if (m_btnDownload.GetSafeHwnd())
    m_btnDownload.EnableWindow(connected && hasSelection);
  if (m_btnDelete.GetSafeHwnd())
    m_btnDelete.EnableWindow(connected && hasSelection);
  if (m_btnRun.GetSafeHwnd())
    m_btnRun.EnableWindow(connected && hasSelection);

  // 鐩戣鎸夐挳鍙湁鍦ㄨ繛鎺ユ椂鎵嶅惎鐢?
  if (m_btnStartMonitor.GetSafeHwnd())
    m_btnStartMonitor.EnableWindow(connected);
  if (m_btnStartRecord.GetSafeHwnd())
    m_btnStartRecord.EnableWindow(connected);

  // 宸ュ叿鏍忔寜閽?
  if (m_toolbar.GetSafeHwnd()) {
    m_toolbar.GetToolBarCtrl().EnableButton(ID_MVC_TOOLBAR_CONNECT, !connected);
    m_toolbar.GetToolBarCtrl().EnableButton(ID_MVC_TOOLBAR_DISCONNECT,
                                            connected);
    m_toolbar.GetToolBarCtrl().EnableButton(ID_MVC_TOOLBAR_REFRESH, connected);
  }
}

CString RemoteControlViewDlg::GetTreeItemPath(HTREEITEM hItem) {
  if (hItem == NULL)
    return _T("");

  // 缁熶竴浠庤妭鐐规暟鎹腑鑾峰彇瀛樺偍鐨勫畬鏁磋矾寰?
  // 淇鍚庯紝鎵€鏈夎妭鐐癸紙鍖呮嫭椹卞姩鍣ㄨ妭鐐癸級閮戒娇鐢?ItemData
  // 瀛樺偍璺緞
  DWORD_PTR data = m_treeDrives.GetItemData(hItem);
  if (data != 0) {
    CString *pPath = reinterpret_cast<CString *>(data);
    return *pPath;
  }

  // 濡傛灉娌℃湁璺緞鏁版嵁锛岃繑鍥炵┖锛堝彧鏈夋牴鑺傜偣"椹卞姩鍣?娌℃湁璺緞鏁版嵁锛?
  // 鏍硅妭鐐逛笉搴旇琚敤浜庤矾寰勬搷浣滐紝鎵€浠ヨ繑鍥炵┖鏄畨鍏ㄧ殑
  return _T("");
}

void RemoteControlViewDlg::DrawStatusLED(CDC *pDC, CRect rect, bool connected) {
  // 缁樺埗鍦嗗舰鎸囩ず鐏?
  CBrush brush(connected ? RGB(0, 255, 0) : RGB(255, 0, 0));
  CBrush *pOldBrush = pDC->SelectObject(&brush);
  pDC->Ellipse(rect);
  pDC->SelectObject(pOldBrush);
}

// ============================================================================
// 娑堟伅澶勭悊鍑芥暟(灏嗗湪涓嬩竴閮ㄥ垎缁х画瀹炵幇)
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

    HICON hIcon = (m_protocol && m_protocol->sysRes) ? m_protocol->sysRes->hIcon
                                                     : nullptr;
    if (hIcon) {
      dc.DrawIcon(x, y, hIcon);
    }

    // 缁樺埗鐘舵€佹寚绀虹伅 (閫氳繃 GetDlgItem 瀹夊叏璁块棶锛岄伩鍏?DDX_Control
    // 瀛愮被鍖栨柇瑷€)
    CWnd *pLed = GetDlgItem(IDC_MVC_LED_STATUS);
    if (pLed && pLed->GetSafeHwnd()) {
      CRect ledRect;
      pLed->GetWindowRect(&ledRect);
      ScreenToClient(&ledRect);
      bool connected = (m_protocol && m_protocol->state)
                           ? m_protocol->state->isConnected
                           : false;
      DrawStatusLED(&dc, ledRect, connected);
    }
  } else {
    CDialogEx::OnPaint();
  }
}

HCURSOR RemoteControlViewDlg::OnQueryDragIcon() {
  HICON hIcon =
      (m_protocol && m_protocol->sysRes) ? m_protocol->sysRes->hIcon : nullptr;
  return static_cast<HCURSOR>(hIcon);
}

void RemoteControlViewDlg::OnSize(UINT nType, int cx, int cy) {
  CDialogEx::OnSize(nType, cx, cy);

  if (nType == SIZE_MINIMIZED || cx <= 0 || cy <= 0)
    return;

  // 保持工具栏和状态栏布局
  if (m_toolbar.GetSafeHwnd() && m_statusBar.GetSafeHwnd()) {
    RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
  }

  // 获取客户区大小
  CRect rcClient;
  GetClientRect(&rcClient);
  if (rcClient.IsRectEmpty())
    return;

  // 如果没有记录初始控件信息，则不做自动调整
  if (!(m_protocol && m_protocol->layout) ||
      m_protocol->layout->originalRects.empty())
    return;

  // 使用基于初始布局的差量调整控件位置和大小
  int clientW = rcClient.Width();
  int clientH = rcClient.Height();
  AdjustControlLayout(clientW, clientH);
}

void RemoteControlViewDlg::AdjustControlLayout(int cx, int cy) {
  if (!(m_protocol && m_protocol->layout))
    return;

  int diffX = cx - m_protocol->layout->originalDialogRect.Width();
  int diffY = cy - m_protocol->layout->originalDialogRect.Height();

  for (const auto &rule : m_protocol->layout->rules) {
    auto it = m_protocol->layout->originalRects.find(rule.nID);
    if (it == m_protocol->layout->originalRects.end())
      continue;

    bool isGroup =
        (rule.nID == IDC_MVC_GROUP_CONNECTION ||
         rule.nID == IDC_MVC_GROUP_FILES || rule.nID == IDC_MVC_GROUP_MONITOR);

    CRect orig = it->second;
    int newLeft = orig.left;
    int newTop = orig.top;
    int newRight = orig.right;
    int newBottom = orig.bottom;

    if (isGroup) {
      if (rule.left && rule.right) {
        newRight = orig.right + diffX;
      }
      if (rule.top && rule.bottom) {
        newBottom = orig.bottom + diffY;
      } else if (!rule.top && rule.bottom) {
        newTop = orig.top + diffY;
        newBottom = orig.bottom + diffY;
      }
    } else {
      if (rule.left && rule.right) {
        newRight = orig.right + diffX;
      } else if (!rule.left && rule.right) {
        newLeft = orig.left + diffX;
        newRight = orig.right + diffX;
      }

      if (rule.top && rule.bottom) {
        newBottom = orig.bottom + diffY;
      } else if (!rule.top && rule.bottom) {
        newTop = orig.top + diffY;
        newBottom = orig.bottom + diffY;
      }
    }

    CWnd *pWnd = GetDlgItem(rule.nID);
    if (pWnd && pWnd->GetSafeHwnd()) {
      CRect newRect(newLeft, newTop, newRight, newBottom);
      pWnd->MoveWindow(&newRect);
    }
  }
}

void RemoteControlViewDlg::OnGetMinMaxInfo(MINMAXINFO *lpMMI) {
  // 设置最小窗口尺寸 (像素) - 匹配紧凑对话框布局 (480x380 对话框单位)
  lpMMI->ptMinTrackSize.x = 600; // 最小宽度
  lpMMI->ptMinTrackSize.y = 500; // 最小高度，确保监视区域可见

  CDialogEx::OnGetMinMaxInfo(lpMMI);
}

// 宸ュ叿鏍忔寜閽鐞?濮旀墭缁欏搴旂殑鎸夐挳澶勭悊鍑芥暟)
void RemoteControlViewDlg::OnToolbarConnect() { OnBnClickedConnect(); }
void RemoteControlViewDlg::OnToolbarDisconnect() { OnBnClickedConnect(); }
void RemoteControlViewDlg::OnToolbarRefresh() { OnBnClickedRefresh(); }
void RemoteControlViewDlg::OnToolbarSettings() { ShowInfo("打开设置"); }
void RemoteControlViewDlg::OnToolbarHelp() { ShowInfo("打开帮助"); }
void RemoteControlViewDlg::OnToolbarAbout() {
  ShowInfo("远程控制 (MVC) 版本 v1.0");
}

// 杩炴帴鎸夐挳
void RemoteControlViewDlg::OnBnClickedConnect() {
  if (!controller_)
    return;

  bool connected = (m_protocol && m_protocol->state)
                       ? m_protocol->state->isConnected
                       : false;
  if (!connected) {
    // 鑾峰彇IP
    std::string ip = "127.0.0.1";
    if (m_ipAddress.GetSafeHwnd()) {
      BYTE b1, b2, b3, b4;
      m_ipAddress.GetAddress(b1, b2, b3, b4);
      std::ostringstream oss;
      oss << (int)b1 << "." << (int)b2 << "." << (int)b3 << "." << (int)b4;
      ip = oss.str();
    }

    // 鑾峰彇绔彛
    uint16_t port = 9527;
    if (m_editPort.GetSafeHwnd()) {
      CString portStr;
      m_editPort.GetWindowText(portStr);
      if (!portStr.IsEmpty()) {
        port = (uint16_t)_ttoi(portStr);
      }
    }

    // 濮旀墭缁機ontroller
    controller_->OnConnectRequested(ip, port);
  } else {
    // 鏂紑杩炴帴
    controller_->OnDisconnectRequested();
  }
}

void RemoteControlViewDlg::OnIpAddressChanged(NMHDR *pNMHDR, LRESULT *pResult) {
  *pResult = 0;
}

void RemoteControlViewDlg::OnPortChanged() {}

// 鏂囦欢娴忚
void RemoteControlViewDlg::OnBnClickedRefresh() {
  if (!controller_)
    return;

  // 濡傛灉鏈夐€変腑鐨勭洰褰?鍒欏埛鏂拌鐩綍
  HTREEITEM hItem = m_treeDrives.GetSelectedItem();
  if (hItem != NULL) {
    CString path = GetTreeItemPath(hItem);
    if (!path.IsEmpty() && path != _T("驱动器")) {
      // 娓呴櫎闃叉姈缂撳瓨,寮哄埗閲嶆柊鍔犺浇
      if (m_protocol && m_protocol->state)
        m_protocol->state->lastSelectedPath = _T("");

      // 閲嶆柊鍔犺浇褰撳墠鐩綍鐨勬枃浠跺垪琛?
      controller_->OnDirectorySelected(std::string(CT2A(path)));

      // 濡傛灉鑺傜偣宸插睍寮€,涔熷埛鏂板瓙鐩綍
      if (m_treeDrives.GetItemState(hItem, TVIS_EXPANDED) & TVIS_EXPANDED) {
        // 鍒犻櫎鎵€鏈夊瓙鑺傜偣,娣诲姞鍗犱綅绗?浠ヤ究閲嶆柊鍔犺浇
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
        // 娣诲姞鍗犱綅绗?
        m_treeDrives.InsertItem(_T(""), hItem);

        // 瑙﹀彂閲嶆柊鍔犺浇锛堣矾寰勪俊鎭細闅忔秷鎭竴璧蜂紶閫掞級
        controller_->OnDirectoryExpanded(std::string(CT2A(path)));
      }
      return;
    }
  }

  // 鍚﹀垯鍒锋柊椹卞姩鍣ㄥ垪琛?
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

  // 鏇存柊褰撳墠璺緞
  if (m_protocol && m_protocol->state) {
    m_protocol->state->currentPath = path;
    m_protocol->state->lastSelectedPath = path;
  }
  m_editPath.SetWindowText(path);

  // 娉ㄦ剰锛氫笉璁剧疆
  // m_hExpandingItem锛屽洜涓洪€夋嫨鑺傜偣涓嶅簲璇ユ洿鏂板瓙鐩綍缁撴瀯
  // 瀛愮洰褰曠粨鏋勫彧鍦ㄥ睍寮€鑺傜偣鏃舵洿鏂帮紙OnTreeItemExpanding锛?

  // 濮旀墭缁機ontroller鍔犺浇鏂囦欢鍒楄〃锛堝彧鏇存柊鍙充晶鍒楄〃锛屼笉鏇存柊宸︿晶鏍戯級
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

  // 濮旀墭缁機ontroller灞曞紑鐩綍
  controller_->OnDirectoryExpanded(std::string(CT2A(path)));
}

void RemoteControlViewDlg::OnTreeItemExpanding(NMHDR *pNMHDR,
                                               LRESULT *pResult) {
  LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
  *pResult = 0;

  // 鍙鐞嗗睍寮€浜嬩欢锛屼笉澶勭悊鎶樺彔
  if (pNMTreeView->action != TVE_EXPAND)
    return;

  HTREEITEM hItem = pNMTreeView->itemNew.hItem;
  if (hItem == NULL)
    return;

  // 鑾峰彇鑺傜偣璺緞
  CString path = GetTreeItemPath(hItem);
  if (path.IsEmpty() || path == _T("驱动器")) {
    return;
  }

  // 娣诲姞璋冭瘯鏃ュ織浠ヨ拷韪睍寮€鎿嶄綔
  TRACE(_T("OnTreeItemExpanding: %s\n"), (LPCTSTR)path);

  // 妫€鏌ユ槸鍚﹀彧鏈夊崰浣嶇瀛愯妭鐐?绌烘枃鏈殑瀛愯妭鐐?
  HTREEITEM hChild = m_treeDrives.GetChildItem(hItem);
  bool hasPlaceholder = false;
  if (hChild != NULL) {
    CString childText = m_treeDrives.GetItemText(hChild);
    if (childText.IsEmpty()) {
      hasPlaceholder = true;
    }
  }

  // 鍙湁褰撳瓨鍦ㄥ崰浣嶇鏃舵墠鍔犺浇(棣栨灞曞紑)
  // 濡傛灉宸茬粡鏈夊疄闄呭唴瀹?璇存槑宸插姞杞借繃,涓嶉噸澶嶅姞杞?
  if (!hasPlaceholder && hChild != NULL) {
    TRACE(_T("  Already loaded, skipping\n"));
    return;
  }

  // 委托给 Controller 处理子目录加载
  if (!controller_) {
    TRACE(_T("  Error: controller_ is null\n"));
    return;
  }

  CT2A convPath(path);
  std::string pathStr(static_cast<const char *>(convPath));
  controller_->OnTreeNodeExpanding(static_cast<void *>(hItem), pathStr);
}

void RemoteControlViewDlg::OnListRightClick(NMHDR *pNMHDR, LRESULT *pResult) {
  LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
  *pResult = 0;

  // 鑾峰彇閫変腑椤?
  POSITION pos = m_listFiles.GetFirstSelectedItemPosition();
  if (pos == NULL)
    return;

  int nItem = m_listFiles.GetNextSelectedItem(pos);
  if (m_protocol && m_protocol->state) {
    m_protocol->state->selectedFile = m_listFiles.GetItemText(nItem, 0);
  }
  UpdateButtonStates();

  // 鏄剧ず鍙抽敭鑿滃崟
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
  // 鍙屽嚮鏂囦欢榛樿涓嬭浇
  OnMenuDownload();
}

// 鏂囦欢鎿嶄綔鎸夐挳
void RemoteControlViewDlg::OnBnClickedUpload() {
  if (!controller_)
    return;

  CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
                  _T("所有文件 (*.*)|*.*||"), this);

  if (dlg.DoModal() == IDOK) {
    CString localPath = dlg.GetPathName();
    CString remotePath =
        (m_protocol && m_protocol->state)
            ? (m_protocol->state->currentPath + _T("\\") + dlg.GetFileName())
            : CString();

    controller_->OnFileUpload(std::string(CT2A(localPath)),
                              std::string(CT2A(remotePath)));
  }
}

void RemoteControlViewDlg::OnBnClickedDownload() { OnMenuDownload(); }

void RemoteControlViewDlg::OnBnClickedDelete() { OnMenuDelete(); }

void RemoteControlViewDlg::OnBnClickedRun() { OnMenuRun(); }

// 鍙抽敭鑿滃崟
void RemoteControlViewDlg::OnMenuDownload() {
  if (!controller_ || !(m_protocol && m_protocol->state) ||
      m_protocol->state->selectedFile.IsEmpty())
    return;

  CFileDialog dlg(
      FALSE, NULL,
      (m_protocol && m_protocol->state) ? m_protocol->state->selectedFile
                                        : CString(),
      OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, _T("所有文件 (*.*)|*.*||"), this);

  if (dlg.DoModal() == IDOK) {
    CString remotePath = (m_protocol && m_protocol->state)
                             ? (m_protocol->state->currentPath + _T("\\") +
                                m_protocol->state->selectedFile)
                             : CString();
    CString localPath = dlg.GetPathName();

    controller_->OnFileDownload(std::string(CT2A(remotePath)),
                                std::string(CT2A(localPath)));
  }
}

void RemoteControlViewDlg::OnMenuUpload() { OnBnClickedUpload(); }

void RemoteControlViewDlg::OnMenuDelete() {
  if (!controller_ || !(m_protocol && m_protocol->state) ||
      m_protocol->state->selectedFile.IsEmpty())
    return;

  CString msg;
  msg.Format(_T("是否确定删除文件 '%s' ?"),
             (LPCTSTR)((m_protocol && m_protocol->state)
                           ? m_protocol->state->selectedFile
                           : CString()));

  if (MessageBox(msg, _T("确认删除"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
    CString remotePath = (m_protocol && m_protocol->state)
                             ? (m_protocol->state->currentPath + _T("\\") +
                                m_protocol->state->selectedFile)
                             : CString();
    controller_->OnFileDelete(std::string(CT2A(remotePath)));
  }
}

void RemoteControlViewDlg::OnMenuRun() {
  if (!controller_ || !(m_protocol && m_protocol->state) ||
      m_protocol->state->selectedFile.IsEmpty())
    return;
  CString remotePath = (m_protocol && m_protocol->state)
                           ? (m_protocol->state->currentPath + _T("\\") +
                              m_protocol->state->selectedFile)
                           : CString();
  controller_->OnFileRun(std::string(CT2A(remotePath)));
}

void RemoteControlViewDlg::OnMenuProperties() {
  ShowInfo("属性对话框尚未实现");
}

// 鐩戣鍔熻兘
void RemoteControlViewDlg::OnBnClickedStartMonitor() {
  if (!controller_)
    return;

  bool monitoring = (m_protocol && m_protocol->state)
                        ? m_protocol->state->isMonitoring
                        : false;
  if (!monitoring) {
    controller_->OnStartMonitor();
    if (m_protocol && m_protocol->state)
      m_protocol->state->isMonitoring = true;
    m_btnStartMonitor.SetWindowText(_T("停止监视"));
    UpdateStatusBar("屏幕监视已启动", 0);
  } else {
    controller_->OnStopMonitor();
    if (m_protocol && m_protocol->state)
      m_protocol->state->isMonitoring = false;
    m_btnStartMonitor.SetWindowText(_T("屏幕监视"));
    UpdateStatusBar("屏幕监视已停止", 0);
  }
}

void RemoteControlViewDlg::OnBnClickedStartRecord() {
  ShowInfo("录制功能尚未实现");
}
