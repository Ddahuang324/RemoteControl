#pragma once

#include "../interfaces/IController.h"
#include "../model/Interface.h"
#include "../view/RemoteControlViewDlg.h"
#include <memory>
#include <string>

// 前向声明
class MonitorViewDlg;
class MonitorController;

// ============================================================================
// MainController: 主窗口Controller实现
// 职责: 协调NetworkModel和FileSystemModel,处理主窗口的业务逻辑
// ============================================================================
class MainController : public IMainController {
public:
  MainController(std::shared_ptr<INetworkModel> network,
                 std::shared_ptr<IFileSystemModel> fileSystem,
                 std::shared_ptr<IMonitorModel> monitor,
                 std::shared_ptr<IIoModel> io, RemoteControlViewDlg *view);

  virtual ~MainController();

  // ---- IMainController接口实现 ----

  void OnConnectRequested(const std::string &ip, uint16_t port) override;
  void OnDisconnectRequested() override;
  void OnRefreshDrives() override;
  void OnDirectorySelected(const std::string &path) override;
  void OnDirectoryExpanded(const std::string &path) override;
  void OnFileDownload(const std::string &remotePath,
                      const std::string &localPath) override;
  void OnFileUpload(const std::string &localPath,
                    const std::string &remotePath) override;
  void OnFileDelete(const std::string &path) override;
  void OnFileRun(const std::string &path) override;
  void OnStartMonitor() override;

private:
  // ---- Model接口 ----
  std::shared_ptr<INetworkModel> network_;
  std::shared_ptr<IFileSystemModel> fileSystem_;
  std::shared_ptr<IMonitorModel> monitor_;
  std::shared_ptr<IIoModel> io_;

  // ---- View ----
  RemoteControlViewDlg *view_; // 原始指针,生命周期由MFC管理

  // ---- 监视窗口 ----
  MonitorViewDlg *monitorView_;
  std::shared_ptr<MonitorController> monitorController_;
};
