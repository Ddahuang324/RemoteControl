#pragma once

#include "Interface.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// FileSystemModel: Model 层的骨架实现，负责文件系统相关的网络交互与传输管理。
// - 提供构造/析构与基本成员
// - 提供简单的 listDrives/listDirectory 回调（空实现）
// - 提供 download/upload 返回可取消的 TransferHandle（异步任务会立即返回“未实现”结果）
// 详细的协议解析与文件 I/O 在后续步骤实现。

class FileSystemModel : public IFileSystemModel, public FileSystemProtocol {
public:
  explicit FileSystemModel(std::shared_ptr<INetworkModel> network,
                           std::shared_ptr<ThreadPool> pool = nullptr);
  ~FileSystemModel() override;

  // IFileSystemModel 接口实现（骨架）
  void listDrives(DrivesCb cb) override;
  void listDirectory(const std::string &path, ListCb cb) override;

  std::shared_ptr<TransferHandle>
  downloadFile(const std::string &remotePath, const std::string &localPath,
               ProgressCb progress, ResultCb result) override;

  std::shared_ptr<TransferHandle>
  uploadFile(const std::string &localPath, const std::string &remotePath,
             ProgressCb progress, ResultCb result) override;

  void cancelTransfer(const std::string &transferId) override;

  void deleteFile(const std::string &path, ResultCb cb) override;
  void runFile(const std::string &path, ResultCb cb) override;

private:
  std::shared_ptr<INetworkModel> network_;
  std::shared_ptr<ThreadPool> pool_;

  // 串行化所有网络操作：确保同一时刻只有一个线程在进行网络交互
  std::mutex networkMutex_;
  
  // 保留 downloadMutex_ 用于向后兼容（可在后续版本移除）
  std::mutex downloadMutex_;

  // 本地 transfer id 计数器
  std::atomic<uint64_t> nextTransferSeq_{1};

  // Helpers
  std::string makeTransferId();
};
