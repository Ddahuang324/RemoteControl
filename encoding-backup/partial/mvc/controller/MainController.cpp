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
// 连接管理
// ============================================================================

void MainController::OnConnectRequested(const std::string &ip, uint16_t port) {
  // 在后台线程执行连接,避免阻塞UI
  std::thread([this, ip, port]() {
    bool success = network_->connectToServer(ip, port);

    // 切回UI线程：通过 PostMessage 将更新请求发给 View（避免直接从后台线程访问
    // MFC 控件）
    if (view_ && ::IsWindow(view_->GetSafeHwnd())) {
      ::PostMessage(view_->GetSafeHwnd(), WM_APP + 0x100,
                    (WPARAM)(success ? 1 : 0), 0);
    }
  }).detach();
}

void MainController::OnDisconnectRequested() {
  // 在后台线程执行断开,避免阻塞UI
  std::thread([this]() {
    network_->disconnect();

    // 切回UI线程：通过 PostMessage 将更新请求发给 View
    if (view_ && ::IsWindow(view_->GetSafeHwnd())) {
      ::PostMessage(view_->GetSafeHwnd(), WM_APP + 0x100, (WPARAM)0, 0);
    }
  }).detach();
}

// ============================================================================
// 文件系统操作
// ============================================================================

void MainController::OnRefreshDrives() {
  fileSystem_->listDrives([this](const std::vector<std::string> &drives) {
    if (view_ && ::IsWindow(view_->GetSafeHwnd())) {
      // 拷贝数据到堆，通过PostMessage传递到UI线程
      auto *pDrives = new std::vector<std::string>(drives);
      ::PostMessage(view_->GetSafeHwnd(), WM_UPDATE_DRIVE_LIST, (WPARAM)pDrives,
                    0);
    }
  });
}

void MainController::OnDirectorySelected(const std::string &path) {
  fileSystem_->listDirectory(
      path, [this](const std::vector<FileSystemProtocol::FileEntry> &entries,
                   bool hasMore) {
        if (view_ && ::IsWindow(view_->GetSafeHwnd())) {
          // 拷贝数据到堆，通过PostMessage传递到UI线程
          // 只更新右侧文件列表，不更新左侧子目录结构
          // 左侧子目录结构只在展开节点时更新（OnDirectoryExpanded）
          auto *pEntriesForFiles =
              new std::vector<FileSystemProtocol::FileEntry>(entries);

          // 更新右侧文件列表
          ::PostMessage(view_->GetSafeHwnd(), WM_UPDATE_FILE_LIST,
                        (WPARAM)pEntriesForFiles, (LPARAM)hasMore);
        }
      });
}

void MainController::OnDirectoryExpanded(const std::string &path) {
  // 展开目录：View 现在直接调用 Model 并捕获 HTREEITEM
  // 这个方法保留用于向后兼容，但实际逻辑已移到 View 层
  // 因为只有 View 知道要更新哪个树节点
}

// ============================================================================
// 文件操作
// ============================================================================

void MainController::OnFileDownload(const std::string &remotePath,
                                    const std::string &localPath) {
  auto handle = fileSystem_->downloadFile(
      remotePath, localPath,
      // 进度回调
      [this, remotePath](int percent) {
        if (view_) {
          // 提取文件名
          size_t pos = remotePath.find_last_of("\\/");
          std::string filename = (pos != std::string::npos)
                                     ? remotePath.substr(pos + 1)
                                     : remotePath;

          view_->UpdateTransferProgress(percent, filename);
        }
      },
      // 结果回调
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
      // 进度回调
      [this, localPath](int percent) {
        if (view_) {
          size_t pos = localPath.find_last_of("\\/");
          std::string filename = (pos != std::string::npos)
                                     ? localPath.substr(pos + 1)
                                     : localPath;

          view_->UpdateTransferProgress(percent, filename);
        }
      },
      // 结果回调
      [this](bool success, const std::string &errmsg) {
        if (view_) {
          if (success) {
            view_->ShowInfo("文件上传成功!");
            view_->UpdateStatusBar("上传完成", 0);
          } else {
            view_->ShowError("文件上传失败: " + errmsg);
            view_->UpdateStatusBar("上传失败", 0);
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
            view_->UpdateStatusBar("文件已删除", 0);

            // 刷新当前目录
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
        view_->ShowInfo("文件已在远程服务器上运行");
        view_->UpdateStatusBar("文件运行成功", 0);
      } else {
        view_->ShowError("文件运行失败: " + errmsg);
      }
    }
  });
}

// ============================================================================
// 监视功能
// ============================================================================

void MainController::OnStartMonitor() {
  // 确保使用正确的资源句柄（对于 EXE 应用程序，使用
  // AfxGetApp()->m_hInstance）
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

  // 创建监视窗口对象 (还未创建窗口句柄)
  monitorView_ = new MonitorViewDlg(monitor_, io_);

  // 创建监视Controller（MonitorController 构造函数仅接受 monitor）
  monitorController_ = std::make_shared<MonitorController>(monitor_);
  // 注入 IOModel 到 Controller
  monitorController_->setIoModel(io_);

  // 注入 Controller 到 View (SetController 应在 Create 之前设置,
  // 以便 View::OnInitDialog 可以访问 controller_)
  if (monitorView_) {
    monitorView_->SetController(monitorController_);
  }

  // 创建并显示窗口 (Create 会触发 OnInitDialog)
  CWnd *pParent = AfxGetMainWnd();
  if (pParent == nullptr) {
    TRACE0("Warning: AfxGetMainWnd returned NULL, fallback to view_ as "
           "parent\n");
    pParent = view_;
  }

  TRACE0("Creating MonitorViewDlg with parent HWND\n");
  if (monitorView_ && monitorView_->Create(IDD_MVC_MONITOR_DIALOG, pParent)) {
    // Create 成功后，窗口句柄已创建，此时把 HWND 告知 Controller
    monitorController_->attachView(monitorView_->GetSafeHwnd());
    monitorView_->ShowWindow(SW_SHOW);

    // 在窗口完全创建后启动屏幕捕获，避免在 OnInitDialog 阶段触发断言
    if (monitorController_) {
      monitorController_->OnStartCapture(30);
    }
  } else {
    delete monitorView_;
    monitorView_ = nullptr;
    monitorController_ = nullptr;

    if (view_) {
      view_->ShowError("创建监视窗口失败");
    }
  }
}
