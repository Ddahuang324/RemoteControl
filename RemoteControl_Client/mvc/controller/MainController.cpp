#include "pch.h"
#include "MainController.h"
#include "../view/MonitorViewDlg.h"
#include "MonitorController.h"
#include <thread>

MainController::MainController(std::shared_ptr<INetworkModel> network,
                               std::shared_ptr<IFileSystemModel> fileSystem,
                               std::shared_ptr<IMonitorModel> monitor,
                               std::shared_ptr<IIoModel> io,
                               RemoteControlViewDlg *view)
    : network_(network), fileSystem_(fileSystem), monitor_(monitor), io_(io),
      view_(view), monitorView_(nullptr), monitorController_(nullptr) {}

MainController::~MainController() {}

// ============================================================================
// 杩炴帴绠＄悊
// ============================================================================

void MainController::OnConnectRequested(const std::string &ip, uint16_t port) {
  // 鍦ㄥ悗鍙扮嚎绋嬫墽琛岃繛鎺?閬垮厤闃诲UI
  std::thread([this, ip, port]() {
    bool success = network_->connectToServer(ip, port);

    // 鍒囧洖UI绾跨▼锛氶€氳繃 PostMessage
    // 灏嗘洿鏂拌姹傚彂缁?View锛堥伩鍏嶇洿鎺ヤ粠鍚庡彴绾跨▼璁块棶
    // MFC 鎺т欢锛?
    if (view_ && ::IsWindow(view_->GetSafeHwnd())) {
      ::PostMessage(view_->GetSafeHwnd(), WM_APP + 0x100,
                    (WPARAM)(success ? 1 : 0), 0);
    }
  }).detach();
}

void MainController::OnDisconnectRequested() {
  // 鍦ㄥ悗鍙扮嚎绋嬫墽琛屾柇寮€,閬垮厤闃诲UI
  std::thread([this]() {
    network_->disconnect();

    // 鍒囧洖UI绾跨▼锛氶€氳繃 PostMessage
    // 灏嗘洿鏂拌姹傚彂缁?View
    if (view_ && ::IsWindow(view_->GetSafeHwnd())) {
      ::PostMessage(view_->GetSafeHwnd(), WM_APP + 0x100, (WPARAM)0, 0);
    }
  }).detach();
}

// ============================================================================
// 鏂囦欢绯荤粺鎿嶄綔
// ============================================================================

void MainController::OnRefreshDrives() {
  fileSystem_->listDrives([this](const std::vector<std::string> &drives) {
    if (view_ && ::IsWindow(view_->GetSafeHwnd())) {
      // 鎷疯礉鏁版嵁鍒板爢锛岄€氳繃PostMessage浼犻€掑埌UI绾跨▼
      auto *pDrives = new std::vector<std::string>(drives);
      ::PostMessage(view_->GetSafeHwnd(), WM_UPDATE_DRIVE_LIST, (WPARAM)pDrives,
                    0);
    }
  });
}

void MainController::OnDirectorySelected(const std::string &path) {
  // 先发送清除消息
  if (view_ && ::IsWindow(view_->GetSafeHwnd())) {
    ::PostMessage(view_->GetSafeHwnd(), WM_UPDATE_FILE_LIST, 0, 0);
  }

  fileSystem_->listDirectory(
      path, [this](const std::vector<FileSystemProtocol::FileEntry> &entries,
                   bool hasMore) {
        if (view_ && ::IsWindow(view_->GetSafeHwnd())) {
          // 筛选文件（列表控件只显示文件）
          auto *pEntriesForFiles =
              new std::vector<FileSystemProtocol::FileEntry>();
          for (const auto &entry : entries) {
            if (!entry.isDirectory) {
              pEntriesForFiles->push_back(entry);
            }
          }

          // 即使为空也发送消息，以便处理 hasMore 状态（例如结束加载动画）
          // 如果 pEntriesForFiles 为空且 hasMore 为 false，View 应该能处理
          ::PostMessage(view_->GetSafeHwnd(), WM_UPDATE_FILE_LIST,
                        (WPARAM)pEntriesForFiles, (LPARAM)hasMore);
        }
      });
}

void MainController::OnDirectoryExpanded(const std::string &path) {
  // 灞曞紑鐩綍锛歏iew 鐜板湪鐩存帴璋冪敤 Model
  // 骞舵崟鑾?HTREEITEM
  // 杩欎釜鏂规硶淇濈暀鐢ㄤ簬鍚戝悗鍏煎锛屼絾瀹為檯閫昏緫宸茬Щ鍒?View
  // 灞? 鍥犱负鍙湁 View 鐭ラ亾瑕佹洿鏂板摢涓爲鑺傜偣
}

// ============================================================================
// 鏂囦欢鎿嶄綔
// ============================================================================

void MainController::OnFileDownload(const std::string &remotePath,
                                    const std::string &localPath) {
  auto handle = fileSystem_->downloadFile(
      remotePath, localPath,
      // 杩涘害鍥炶皟
      [this, remotePath](int percent) {
        if (view_) {
          // 鎻愬彇鏂囦欢鍚?
          size_t pos = remotePath.find_last_of("\\/");
          std::string filename = (pos != std::string::npos)
                                     ? remotePath.substr(pos + 1)
                                     : remotePath;

          view_->UpdateTransferProgress(percent, filename);
        }
      },
      // 缁撴灉鍥炶皟
      [this](bool success, const std::string &errmsg) {
        if (view_) {
          if (success) {
            view_->ShowInfo("文件下载成功!");
            view_->UpdateStatusBar("下载完成", 0);
          } else {
            view_->ShowError("文件下载失败: " + errmsg);
            view_->UpdateStatusBar("下载失败", 0);
          }
        }
      });
}

void MainController::OnFileUpload(const std::string &localPath,
                                  const std::string &remotePath) {
  auto handle = fileSystem_->uploadFile(
      localPath, remotePath,
      // 杩涘害鍥炶皟
      [this, localPath](int percent) {
        if (view_) {
          size_t pos = localPath.find_last_of("\\/");
          std::string filename = (pos != std::string::npos)
                                     ? localPath.substr(pos + 1)
                                     : localPath;

          view_->UpdateTransferProgress(percent, filename);
        }
      },
      // 缁撴灉鍥炶皟
      [this](bool success, const std::string &errmsg) {
        if (view_) {
          if (success) {
            view_->ShowInfo("文件上传成功!");
            view_->UpdateStatusBar("上传完成", 0);
          } else {
            view_->ShowError("文件上传失败: " + errmsg);
            view_->UpdateStatusBar("涓婁紶澶辫触", 0);
          }
        }
      });
}

void MainController::OnFileDelete(const std::string &path) {
  fileSystem_->deleteFile(
      path, [this, path](bool success, const std::string &errmsg) {
        if (view_) {
          if (success) {
            view_->ShowInfo("文件删除成功!");
            view_->UpdateStatusBar("删除完成", 0);

            // 鍒锋柊褰撳墠鐩綍
            size_t pos = path.find_last_of("\\/");
            if (pos != std::string::npos) {
              std::string dir = path.substr(0, pos);
              OnDirectorySelected(dir);
            }
          } else {
            view_->ShowError("文件删除失败: " + errmsg);
          }
        }
      });
}

void MainController::OnFileRun(const std::string &path) {
  fileSystem_->runFile(path, [this](bool success, const std::string &errmsg) {
    if (view_) {
      if (success) {
        view_->ShowInfo("文件正在服务器上执行");
        view_->UpdateStatusBar("文件运行成功", 0);
      } else {
        view_->ShowError("文件运行失败: " + errmsg);
      }
    }
  });
}

// ============================================================================
// 鐩戣鍔熻兘
// ============================================================================

void MainController::OnStartMonitor() {
  // 纭繚浣跨敤姝ｇ‘鐨勮祫婧愬彞鏌勶紙瀵逛簬 EXE
  // 搴旂敤绋嬪簭锛屼娇鐢? AfxGetApp()->m_hInstance锛?
  HINSTANCE hInst =
      AfxGetApp() ? AfxGetApp()->m_hInstance : AfxGetInstanceHandle();
  if (hInst != nullptr) {
    AfxSetResourceHandle(hInst);
  }

  if (monitorView_ != nullptr) {
    if (::IsWindow(monitorView_->GetSafeHwnd())) {
      ::SetForegroundWindow(monitorView_->GetSafeHwnd());
      return;
    }

    delete monitorView_;
    monitorView_ = nullptr;
    monitorController_.reset();
  }

  // 鍒涘缓鐩戣绐楀彛瀵硅薄 (杩樻湭鍒涘缓绐楀彛鍙ユ焺)
  monitorView_ = new MonitorViewDlg(monitor_, io_);

  // 鍒涘缓鐩戣Controller锛圡onitorController 鏋勯€犲嚱鏁颁粎鎺ュ彈
  // monitor锛?
  monitorController_ = std::make_shared<MonitorController>(monitor_);
  // 娉ㄥ叆 IOModel 鍒?Controller
  monitorController_->setIoModel(io_);

  // 娉ㄥ叆 Controller 鍒?View (SetController 搴斿湪 Create 涔嬪墠璁剧疆,
  // 浠ヤ究 View::OnInitDialog 鍙互璁块棶 controller_)
  if (monitorView_) {
    monitorView_->SetController(monitorController_);
  }

  // 鍒涘缓骞舵樉绀虹獥鍙?(Create 浼氳Е鍙?OnInitDialog)
  CWnd *pParent = AfxGetMainWnd();
  if (pParent == nullptr) {
    TRACE0("Warning: AfxGetMainWnd returned NULL, fallback to view_ as "
           "parent\n");
    pParent = view_;
  }

  TRACE0("Creating MonitorViewDlg with parent HWND\n");
  if (monitorView_ && monitorView_->Create(IDD_MVC_MONITOR_DIALOG, pParent)) {
    // Create 鎴愬姛鍚庯紝绐楀彛鍙ユ焺宸插垱寤猴紝姝ゆ椂鎶?HWND 鍛婄煡
    // Controller
    monitorController_->attachView(monitorView_->GetSafeHwnd());
    monitorView_->ShowWindow(SW_SHOW);

    // 鍦ㄧ獥鍙ｅ畬鍏ㄥ垱寤哄悗鍚姩灞忓箷鎹曡幏锛岄伩鍏嶅湪 OnInitDialog
    // 闃舵瑙﹀彂鏂█
    if (monitorController_) {
      monitorController_->OnStartCapture(30);
    }
  } else {
    delete monitorView_;
    monitorView_ = nullptr;
    monitorController_ = nullptr;

    if (view_) {
      view_->ShowError("鍒涘缓鐩戣绐楀彛澶辫触");
    }
  }
}
