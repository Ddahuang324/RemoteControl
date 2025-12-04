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
    : monitorController_(nullptr) {
  // 初始化协议容器并注入依赖
  m_protocol = std::make_unique<ControllerProtocol::MainControllerProtocol>();
  m_protocol->netModel = network;
  m_protocol->fsModel = fileSystem;
  m_protocol->monitorModel = monitor;
  m_protocol->ioModel = io;
  m_protocol->mainView = view;
  m_protocol->monitorView = nullptr;
}

MainController::~MainController() {}

// ============================================================================
// 杩炴帴绠＄悊
// ============================================================================

void MainController::OnConnectRequested(const std::string &ip, uint16_t port) {
  // 在后台线程执行连接，避免阻塞 UI
  std::thread([this, ip, port]() {
    bool success = false;
    if (m_protocol && m_protocol->netModel) {
      success = m_protocol->netModel->connectToServer(ip, port);
      m_protocol->lastConnectedIP = ip;
      m_protocol->lastConnectedPort = port;
      m_protocol->isConnecting = false;
    }

    if (m_protocol && m_protocol->mainView && ::IsWindow(m_protocol->mainView->GetSafeHwnd())) {
      ::PostMessage(m_protocol->mainView->GetSafeHwnd(), WM_APP + 0x100, (WPARAM)(success ? 1 : 0), 0);
    }
  }).detach();
}

void MainController::OnDisconnectRequested() {
  // 在后台断开连接并通知 UI
  std::thread([this]() {
    if (m_protocol && m_protocol->netModel) {
      m_protocol->netModel->disconnect();
    }

    if (m_protocol && m_protocol->mainView && ::IsWindow(m_protocol->mainView->GetSafeHwnd())) {
      ::PostMessage(m_protocol->mainView->GetSafeHwnd(), WM_APP + 0x100, (WPARAM)0, 0);
    }
  }).detach();
}

// ============================================================================
// 鏂囦欢绯荤粺鎿嶄綔
// ============================================================================

void MainController::OnRefreshDrives() {
  if (m_protocol && m_protocol->fsModel) {
    m_protocol->fsModel->listDrives([this](const std::vector<std::string> &drives) {
      if (m_protocol && m_protocol->mainView && ::IsWindow(m_protocol->mainView->GetSafeHwnd())) {
        auto *pDrives = new std::vector<std::string>(drives);
        ::PostMessage(m_protocol->mainView->GetSafeHwnd(), WM_UPDATE_DRIVE_LIST, (WPARAM)pDrives, 0);
      }
    });
  }
}

void MainController::OnDirectorySelected(const std::string &path) {
  // 先发送清除消息
  if (m_protocol && m_protocol->mainView && ::IsWindow(m_protocol->mainView->GetSafeHwnd())) {
    ::PostMessage(m_protocol->mainView->GetSafeHwnd(), WM_UPDATE_FILE_LIST, 0, 0);
  }

  if (m_protocol && m_protocol->fsModel) {
    m_protocol->fsModel->listDirectory(path, [this](const std::vector<FileSystemProtocol::FileEntry> &entries, bool hasMore) {
      if (m_protocol && m_protocol->mainView && ::IsWindow(m_protocol->mainView->GetSafeHwnd())) {
        auto *pEntriesForFiles = new std::vector<FileSystemProtocol::FileEntry>();
        for (const auto &entry : entries) {
          if (!entry.isDirectory) {
            pEntriesForFiles->push_back(entry);
          }
        }
        ::PostMessage(m_protocol->mainView->GetSafeHwnd(), WM_UPDATE_FILE_LIST, (WPARAM)pEntriesForFiles, (LPARAM)hasMore);
      }
    });
  }
}

void MainController::OnDirectoryExpanded(const std::string &path) {
  // 灞曞紑鐩綍锛歏iew 鐜板湪鐩存帴璋冪敤 Model
  // 骞舵崟鑾?HTREEITEM
  // 杩欎釜鏂规硶淇濈暀鐢ㄤ簬鍚戝悗鍏煎锛屼絾瀹為檯閫昏緫宸茬Щ鍒?View
  // 灞? 鍥犱负鍙湁 View 鐭ラ亾瑕佹洿鏂板摢涓爲鑺傜偣
}

void MainController::OnTreeNodeExpanding(void* hTreeItem, const std::string &path) {
  HTREEITEM hItem = static_cast<HTREEITEM>(hTreeItem);

  if (!(m_protocol && m_protocol->fsModel)) {
    TRACE(_T("  Error: fsModel is null in OnTreeNodeExpanding\n"));
    return;
  }

  HWND hWnd = (m_protocol && m_protocol->mainView) ? m_protocol->mainView->GetSafeHwnd() : nullptr;
  if (!::IsWindow(hWnd)) return;

  auto isFirst = std::make_shared<bool>(true);
  m_protocol->fsModel->listDirectory(
      path,
      [hWnd, hItem, isFirst](const std::vector<FileSystemProtocol::FileEntry> &entries, bool hasMore) {
        if (!::IsWindow(hWnd)) return;

        auto *pData = new SubDirUpdateData();
        pData->hParent = hItem;
        pData->entries = entries;
        pData->isFirst = *isFirst;
        *isFirst = false;

        ::PostMessage(hWnd, WM_UPDATE_SUB_DIRS, (WPARAM)pData, 0);
      });
}

// ============================================================================
// 鏂囦欢鎿嶄綔
// ============================================================================

void MainController::OnFileDownload(const std::string &remotePath,
                                    const std::string &localPath) {
  if (m_protocol && m_protocol->fsModel) {
    auto handle = m_protocol->fsModel->downloadFile(
        remotePath, localPath,
        [this, remotePath](int percent) {
          if (m_protocol && m_protocol->mainView) {
            size_t pos = remotePath.find_last_of("\\/");
            std::string filename = (pos != std::string::npos) ? remotePath.substr(pos + 1) : remotePath;
            m_protocol->mainView->UpdateTransferProgress(percent, filename);
          }
        },
        [this](bool success, const std::string &errmsg) {
          if (m_protocol && m_protocol->mainView) {
            if (success) {
              m_protocol->mainView->ShowInfo("文件下载成功!");
              m_protocol->mainView->UpdateStatusBar("下载完成", 0);
            } else {
              m_protocol->mainView->ShowError("文件下载失败: " + errmsg);
              m_protocol->mainView->UpdateStatusBar("下载失败", 0);
            }
          }
        });
    (void)handle;
  }
}

void MainController::OnFileUpload(const std::string &localPath,
                                  const std::string &remotePath) {
  if (m_protocol && m_protocol->fsModel) {
    auto handle = m_protocol->fsModel->uploadFile(
        localPath, remotePath,
        [this, localPath](int percent) {
          if (m_protocol && m_protocol->mainView) {
            size_t pos = localPath.find_last_of("\\/");
            std::string filename = (pos != std::string::npos) ? localPath.substr(pos + 1) : localPath;
            m_protocol->mainView->UpdateTransferProgress(percent, filename);
          }
        },
        [this](bool success, const std::string &errmsg) {
          if (m_protocol && m_protocol->mainView) {
            if (success) {
              m_protocol->mainView->ShowInfo("文件上传成功!");
              m_protocol->mainView->UpdateStatusBar("上传完成", 0);
            } else {
              m_protocol->mainView->ShowError("文件上传失败: " + errmsg);
              m_protocol->mainView->UpdateStatusBar("上传失败", 0);
            }
          }
        });
    (void)handle;
  }
}

void MainController::OnFileDelete(const std::string &path) {
  if (m_protocol && m_protocol->fsModel) {
    m_protocol->fsModel->deleteFile(path, [this, path](bool success, const std::string &errmsg) {
      if (m_protocol && m_protocol->mainView) {
        if (success) {
          m_protocol->mainView->ShowInfo("文件删除成功!");
          m_protocol->mainView->UpdateStatusBar("删除完成", 0);
          size_t pos = path.find_last_of("\\/");
          if (pos != std::string::npos) {
            std::string dir = path.substr(0, pos);
            OnDirectorySelected(dir);
          }
        } else {
          m_protocol->mainView->ShowError("文件删除失败: " + errmsg);
        }
      }
    });
  }
}

void MainController::OnFileRun(const std::string &path) {
  if (m_protocol && m_protocol->fsModel) {
    m_protocol->fsModel->runFile(path, [this](bool success, const std::string &errmsg) {
      if (m_protocol && m_protocol->mainView) {
        if (success) {
          m_protocol->mainView->ShowInfo("文件正在服务器上执行");
          m_protocol->mainView->UpdateStatusBar("文件运行成功", 0);
        } else {
          m_protocol->mainView->ShowError("文件运行失败: " + errmsg);
        }
      }
    });
  }
}

// ============================================================================
// 鐩戣鍔熻兘
// ============================================================================

void MainController::OnStartMonitor() {
  // 确保资源句柄为当前进程（可在插件或 EXE 情况下需要）
  HINSTANCE hInst = AfxGetApp() ? AfxGetApp()->m_hInstance : AfxGetInstanceHandle();
  if (hInst != nullptr) {
    AfxSetResourceHandle(hInst);
  }

  // 如果已有监视窗口，尝试激活或重建
  if (m_protocol && m_protocol->monitorView != nullptr) {
    if (::IsWindow(m_protocol->monitorView->GetSafeHwnd())) {
      ::SetForegroundWindow(m_protocol->monitorView->GetSafeHwnd());
      return;
    }

    delete m_protocol->monitorView;
    m_protocol->monitorView = nullptr;
    monitorController_.reset();
  }

  // 创建监视窗口实例
  m_protocol->monitorView = new MonitorViewDlg();

  monitorController_ = std::make_shared<MonitorController>(m_protocol ? m_protocol->monitorModel : nullptr);
  monitorController_->setIoModel(m_protocol ? m_protocol->ioModel : nullptr);

  if (m_protocol && m_protocol->monitorView) {
    m_protocol->monitorView->SetController(monitorController_);
  }

  CWnd *pParent = AfxGetMainWnd();
  if (pParent == nullptr) {
    TRACE0("Warning: AfxGetMainWnd returned NULL, fallback to mainView as parent\n");
    pParent = (m_protocol ? m_protocol->mainView : nullptr);
  }

  TRACE0("Creating MonitorViewDlg with parent HWND\n");
  if (m_protocol && m_protocol->monitorView && m_protocol->monitorView->Create(IDD_MVC_MONITOR_DIALOG, pParent)) {
    monitorController_->attachView(m_protocol->monitorView->GetSafeHwnd());
    m_protocol->monitorView->ShowWindow(SW_SHOW);

    if (monitorController_) {
      monitorController_->OnStartCapture(30);
    }
  } else {
    if (m_protocol && m_protocol->monitorView) {
      delete m_protocol->monitorView;
      m_protocol->monitorView = nullptr;
    }
    monitorController_ = nullptr;

    if (m_protocol && m_protocol->mainView) {
      m_protocol->mainView->ShowError("鍒涘缓鐩戣绐楀彛澶辫触");
    }
  }
}

void MainController::OnStopMonitor() {
  // 停止屏幕捕获
  if (monitorController_) {
    monitorController_->OnStopCapture();
  }
  // 关闭监视窗口
  if (m_protocol && m_protocol->monitorView != nullptr && ::IsWindow(m_protocol->monitorView->GetSafeHwnd())) {
    m_protocol->monitorView->DestroyWindow();
  }

  // 清理资源
  if (m_protocol && m_protocol->monitorView) {
    delete m_protocol->monitorView;
    m_protocol->monitorView = nullptr;
  }
  monitorController_.reset();
}
