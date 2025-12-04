#include "pch.h"
#include "FileSystemModel.h"
#include "../../Enities.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>

// 简单的 TransferHandle 实现
class SimpleTransferHandle : public IFileSystemModel::TransferHandle {
public:
  SimpleTransferHandle(std::string id, std::function<void()> cancelCb)
      : id_(std::move(id)), cancelCb_(std::move(cancelCb)) {}
  std::string id() const override { return id_; }
  void cancel() override {
    if (cancelCb_)
      cancelCb_();
  }

private:
  std::string id_;
  std::function<void()> cancelCb_;
};

FileSystemModel::FileSystemModel(std::shared_ptr<INetworkModel> network,
                                 std::shared_ptr<ThreadPool> pool)
    : network_(std::move(network)), pool_(std::move(pool)) {}

FileSystemModel::~FileSystemModel() {
  // 标记全部传输为取消以便清理（保守策略）
  if (m_fileResources) {
    std::lock_guard<std::mutex> lk(m_fileResources->transfersMutex);
    for (auto &p : m_fileResources->activeTransfers) {
      p.second = true;
    }
  }
}

std::string FileSystemModel::makeTransferId() {
  uint64_t seq = nextTransferSeq_.fetch_add(1);
  return "tx-" + std::to_string(seq);
}

void FileSystemModel::listDrives(DrivesCb cb) {
  if (!cb)
    return;
  if (!network_) {
    cb(std::vector<std::string>());
    return;
  }

  // 在后台线程执行网络IO，避免阻塞UI主线程
  std::thread([this, cb]() {
    // 串行化网络操作：确保同一时刻只有一个线程在进行网络交互
    std::lock_guard<std::mutex> lk(this->networkMutex_);

    // 发送请求包
    Packet req(static_cast<WORD>(CMD::CMD_DRIVER_INFO), std::vector<BYTE>());
    if (!network_->sendPacket(req)) {
      cb(std::vector<std::string>());
      return;
    }

    // 阻塞等待响应（现在在后台线程，不阻塞UI）
    auto pktOpt = network_->getNextPacketBlocking();
    if (!pktOpt) {
      cb(std::vector<std::string>());
      return;
    }
    Packet pkt = *pktOpt;
    if (pkt.sCmd == static_cast<WORD>(CMD::CMD_ERROR)) {
      cb(std::vector<std::string>());
      return;
    }
    if (pkt.sCmd != static_cast<WORD>(CMD::CMD_DRIVER_INFO)) {
      cb(std::vector<std::string>());
      return;
    }

    std::string drives;
    if (!pkt.data.empty())
      drives.assign(pkt.data.begin(), pkt.data.end());

    // 标准化分隔符为逗号
    for (char &ch : drives) {
      if (ch == ';' || std::isspace(static_cast<unsigned char>(ch)))
        ch = ',';
    }

    std::set<std::string> seen;
    std::vector<std::string> result;
    std::stringstream ss(drives);
    std::string token;
    while (std::getline(ss, token, ',')) {
      // trim
      auto ltrim = [](std::string &s) {
        s.erase(s.begin(),
                std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                  return !std::isspace(ch);
                }));
      };
      auto rtrim = [](std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(),
                             [](unsigned char ch) { return !std::isspace(ch); })
                    .base(),
                s.end());
      };
      ltrim(token);
      rtrim(token);
      if (token.empty())
        continue;

      char letter = '\0';

      for (char c : token) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
          letter =
              static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
          break;
        }
      }

      if (letter == '\0')
        continue;
      std::string formatted;
      formatted.push_back(letter);
      formatted += ":\\";
      if (seen.insert(formatted).second)
        result.push_back(formatted);
    }

    cb(result); // 回调在后台线程执行，但会通过PostMessage切回UI线程
  }).detach();
}

void FileSystemModel::listDirectory(const std::string &path, ListCb cb) {
  if (!cb)
    return;
  if (!network_) {
    cb(std::vector<FileEntry>(), false);
    return;
  }

  // 在后台线程执行网络IO，避免阻塞UI主线程
  std::thread([this, path, cb]() {
    // 串行化网络操作：确保同一时刻只有一个线程在进行网络交互
    // 这解决了多个目录请求并发时数据包错乱的问题
    std::lock_guard<std::mutex> lk(this->networkMutex_);

    // 发送目录请求，包体为 path 字符串
    std::vector<BYTE> pathData(path.begin(), path.end());
    Packet req(static_cast<WORD>(CMD::CMD_DIRECTORY_INFO), pathData);
    if (!network_->sendPacket(req)) {
      cb(std::vector<FileEntry>(), false);
      return;
    }

    while (true) {
      auto pktOpt = network_->getNextPacketBlocking();
      if (!pktOpt) {
        // 网络错误
        cb(std::vector<FileEntry>(), false);
        return;
      }
      Packet pkt = *pktOpt;
      if (pkt.sCmd == static_cast<WORD>(CMD::CMD_ERROR)) {
        cb(std::vector<FileEntry>(), false);
        return;
      }
      if (pkt.sCmd != static_cast<WORD>(CMD::CMD_DIRECTORY_INFO)) {
        cb(std::vector<FileEntry>(), false);
        return;
      }

      auto finfoOpt = File_Info::Deserialize(pkt.data);
      if (!finfoOpt) {
        cb(std::vector<FileEntry>(), false);
        return;
      }
      const File_Info &finfo = *finfoOpt;

      // 终止包判定：空路径且非目录且无后续
      // 服务端发送的结束包：isDir=false, fullPath="", hasNext=false
      const bool isEndPacket =
          !finfo.hasNext && !finfo.isDir && finfo.fullPath.empty();
      if (isEndPacket) {
        // 收到结束包，通知完成
        cb(std::vector<FileEntry>(), false);
        break;
      }

      // 转换为 FileEntry
      FileEntry entry;
      entry.fullPath = finfo.fullPath;
      // 取 name 从 fullPath 的最后一段
      auto pos = finfo.fullPath.find_last_of("/\\");
      if (pos == std::string::npos)
        entry.name = finfo.fullPath;
      else
        entry.name = finfo.fullPath.substr(pos + 1);
      entry.isDirectory = finfo.isDir;
      entry.size = 0;

      // 流式回调：每收到一个条目（或一批）就回调一次
      // hasMore = true 表示还有数据（即使这是最后一个条目，因为我们依赖
      // EndPacket 来结束）
      std::vector<FileEntry> batch;
      batch.push_back(std::move(entry));
      cb(batch, true);
    }
  }).detach();
}

std::shared_ptr<IFileSystemModel::TransferHandle>
FileSystemModel::downloadFile(const std::string &remotePath,
                              const std::string &localPath, ProgressCb progress,
                              ResultCb result) {
  std::string tid = makeTransferId();

  // 注册传输
  if (m_fileResources) {
    std::lock_guard<std::mutex> lk(m_fileResources->transfersMutex);
    m_fileResources->activeTransfers[tid] = false; // not cancelled
  }

  auto cancelCb = [this, tid]() { this->cancelTransfer(tid); };
  auto handle = std::make_shared<SimpleTransferHandle>(tid, cancelCb);

  // 异步执行下载任务
  std::thread([this, tid, remotePath, localPath, progress, result]() {
    // 串行化网络操作：确保同一时刻只有一个线程在进行网络交互
    // 使用统一的 networkMutex_ 避免与 listDirectory 等操作冲突
    std::lock_guard<std::mutex> dlk(this->networkMutex_);
    // 1) 发送下载请求（路径作为数据）
    std::vector<BYTE> pathData(remotePath.begin(), remotePath.end());
    Packet req(static_cast<WORD>(CMD::CMD_DOWNLOAD_FILE), pathData);
    if (!network_ || !network_->sendPacket(req)) {
      if (result)
        result(false, "send request failed");
      // 清理注册表
      if (m_fileResources) {
        std::lock_guard<std::mutex> lk(m_fileResources->transfersMutex);
        m_fileResources->activeTransfers.erase(tid);
      }
      return;
    }

    // 2) 接收文件大小包
    auto sizePktOpt =
        network_ ? network_->getNextPacketBlocking() : std::optional<Packet>{};
    if (!sizePktOpt ||
        sizePktOpt->sCmd != static_cast<WORD>(CMD::CMD_DOWNLOAD_FILE)) {
      if (result)
        result(false, "failed to receive size packet");
      if (m_fileResources) {
        std::lock_guard<std::mutex> lk(m_fileResources->transfersMutex);
        m_fileResources->activeTransfers.erase(tid);
      }
      return;
    }

    // 反序列化文件大小（与服务器端一致）
    std::streamsize fileSize = 0;
    if (sizePktOpt->data.size() >= sizeof(std::streamsize)) {
      memcpy(&fileSize, sizePktOpt->data.data(), sizeof(std::streamsize));
    } else {
      if (result)
        result(false, "invalid size packet");
      if (m_fileResources) {
        std::lock_guard<std::mutex> lk(m_fileResources->transfersMutex);
        m_fileResources->activeTransfers.erase(tid);
      }
      return;
    }

    // 3) 准备写文件
    std::ofstream ofs(localPath, std::ios::binary);
    if (!ofs.is_open()) {
      if (result)
        result(false, "failed to open local file");
      if (m_fileResources) {
        std::lock_guard<std::mutex> lk(m_fileResources->transfersMutex);
        m_fileResources->activeTransfers.erase(tid);
      }
      return;
    }

    uint64_t received = 0;

    // 4) 接收循环
    while (true) {
      auto pktOpt = network_ ? network_->getNextPacketBlocking()
                             : std::optional<Packet>{};
      if (!pktOpt) {
        if (result)
          result(false, "network recv error");
        break;
      }
      Packet pkt = *pktOpt;
      if (pkt.sCmd == static_cast<WORD>(CMD::CMD_EOF)) {
        // 完成
        if (progress)
          progress(100);
        if (result)
          result(true, "");
        break;
      }
      if (pkt.sCmd == static_cast<WORD>(CMD::CMD_ERROR)) {
        std::string err(pkt.data.begin(), pkt.data.end());
        if (result)
          result(false, err);
        break;
      }
      if (pkt.sCmd != static_cast<WORD>(CMD::CMD_DOWNLOAD_FILE)) {
        if (result)
          result(false, "unexpected packet cmd");
        break;
      }

      // 检查取消状态
      bool cancelled = false;
      if (m_fileResources) {
        std::lock_guard<std::mutex> lk(m_fileResources->transfersMutex);
        auto it = m_fileResources->activeTransfers.find(tid);
        if (it != m_fileResources->activeTransfers.end())
          cancelled = it->second;
      }
      if (cancelled) {
        if (result)
          result(false, "cancelled");
        break;
      }

      // 写入文件（保护写入互斥）
      if (m_fileResources) {
        std::lock_guard<std::mutex> lk(m_fileResources->fileIoMutex);
        ofs.write(reinterpret_cast<const char *>(pkt.data.data()),
                  pkt.data.size());
      } else {
        ofs.write(reinterpret_cast<const char *>(pkt.data.data()),
                  pkt.data.size());
      }

      received += pkt.data.size();
      if (fileSize > 0 && progress) {
        int percent = static_cast<int>((received * 100) /
                                       static_cast<uint64_t>(fileSize));
        if (percent > 100)
          percent = 100;
        progress(percent);
      }
    }

    // 关闭文件与清理注册表
    ofs.close();
    if (m_fileResources) {
      std::lock_guard<std::mutex> lk(m_fileResources->transfersMutex);
      m_fileResources->activeTransfers.erase(tid);
    }
  }).detach();

  return handle;
}

std::shared_ptr<IFileSystemModel::TransferHandle>
FileSystemModel::uploadFile(const std::string &localPath,
                            const std::string &remotePath, ProgressCb progress,
                            ResultCb result) {
  // 服务器目前不支持客户端上传文件（无对应服务端实现），
  // 因此短期内返回未实现。后续可在 server
  // 实现对应命令后补充。
  if (result)
    result(false, "upload not supported");
  return nullptr;
}

void FileSystemModel::cancelTransfer(const std::string &transferId) {
  if (!m_fileResources)
    return;
  std::lock_guard<std::mutex> lk(m_fileResources->transfersMutex);
  auto it = m_fileResources->activeTransfers.find(transferId);
  if (it != m_fileResources->activeTransfers.end()) {
    it->second = true;
  }
}

void FileSystemModel::deleteFile(const std::string &path, ResultCb cb) {
  if (!cb)
    return;
  if (!network_) {
    cb(false, "no network");
    return;
  }

  // 在后台线程执行网络IO，避免阻塞UI主线程
  std::thread([this, path, cb]() {
    // 串行化网络操作
    std::lock_guard<std::mutex> lk(this->networkMutex_);

    std::vector<BYTE> pathData(path.begin(), path.end());
    Packet req(static_cast<WORD>(CMD::CMD_DELETE_FILE), pathData);
    if (!network_->sendPacket(req)) {
      cb(false, "send failed");
      return;
    }

    auto pktOpt = network_->getNextPacketBlocking();
    if (!pktOpt) {
      cb(false, "no response");
      return;
    }
    Packet pkt = *pktOpt;
    if (pkt.sCmd == static_cast<WORD>(CMD::CMD_DELETE_FILE)) {
      cb(true, "");
    } else if (pkt.sCmd == static_cast<WORD>(CMD::CMD_ERROR)) {
      std::string err(pkt.data.begin(), pkt.data.end());
      cb(false, err);
    } else {
      cb(false, "unexpected response");
    }
  }).detach();
}

void FileSystemModel::runFile(const std::string &path, ResultCb cb) {
  if (!cb)
    return;
  if (!network_) {
    cb(false, "no network");
    return;
  }

  // 在后台线程执行网络IO，避免阻塞UI主线程
  std::thread([this, path, cb]() {
    // 串行化网络操作
    std::lock_guard<std::mutex> lk(this->networkMutex_);

    std::vector<BYTE> pathData(path.begin(), path.end());
    Packet req(static_cast<WORD>(CMD::CMD_RUN_FILE), pathData);
    if (!network_->sendPacket(req)) {
      cb(false, "send failed");
      return;
    }

    auto pktOpt = network_->getNextPacketBlocking();
    if (!pktOpt) {
      cb(false, "no response");
      return;
    }
    Packet pkt = *pktOpt;
    if (pkt.sCmd == static_cast<WORD>(CMD::CMD_RUN_FILE)) {
      cb(true, "");
    } else if (pkt.sCmd == static_cast<WORD>(CMD::CMD_ERROR)) {
      std::string err(pkt.data.begin(), pkt.data.end());
      cb(false, err);
    } else {
      cb(false, "unexpected response");
    }
  }).detach();
}
