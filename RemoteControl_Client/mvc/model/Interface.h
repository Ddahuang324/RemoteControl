#pragma once

#include "../../include/Infra/Packet.hpp"
#include "../../include/Infra/Socket.hpp"
#include "../../include/Protocol/Infra/PacketProtocol.h"
#include "../../include/Protocol/Infra/SocketProtocol.h"
#include "../../include/Protocol/MVC/model/InterfaceProtocol.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>


// 基础类型别名（与 clientSocket.h 中的类型对应）
using BYTE = uint8_t;
using WORD = uint16_t;
using DWORD = unsigned long; // 修改为 unsigned long 以匹配 Windows 定义

// IModel 基类（不再继承 InterfaceProtocol，改为按需组合）
class IModel {
public:
  virtual ~IModel() = default;
};

// ---------------- INetworkModel ----------------
class INetworkModel : public IModel, public NetworkProtocol {
public:
  using PacketCb = std::function<void(const Packet &)>;
  using StatusCb = std::function<void(bool connected)>;

  virtual ~INetworkModel() = default;

  // 连接/断开
  virtual bool connectToServer(const std::string &ip, uint16_t port) = 0;
  virtual void disconnect() = 0;

  // 发送/接收（同步/异步接口混合）
  virtual bool sendPacket(const Packet &pkt) = 0;
  virtual std::optional<Packet>
  recvPacket() = 0; // 非阻塞，若无可用返回 nullopt

  // 阻塞/轮询式获取
  virtual std::optional<Packet>
  getNextPacketBlocking(int timeoutMs = 5000) = 0;
  virtual std::optional<Packet> getLatestPacket() = 0; // 获取最新，丢弃旧帧

  // 缓冲/队列控制
  virtual void clearRecvBuffer() = 0;
  virtual void clearPacketsByCmd(WORD cmd) = 0;
  virtual void clearAllPackets() = 0;

  // 回调注册（实现请保证回调在约定线程中触发）
  virtual void setOnPacketReceived(PacketCb cb) = 0;
  virtual void setOnStatusChanged(StatusCb cb) = 0;
};

// ---------------- IFileSystemModel ----------------
class IFileSystemModel : public IModel, public NetworkProtocol {
public:
  // 传输句柄：调用方可保存并用于取消或查询（当前只定义 cancel 和 id）
  struct TransferHandle {
    virtual ~TransferHandle() = default;
    virtual std::string id() const = 0;
    virtual void cancel() = 0;
  };

  using ListCb = std::function<void(
      const std::vector<FileSystemProtocol::FileEntry> &entries, bool hasMore)>;
  using DrivesCb = std::function<void(const std::vector<std::string> &drives)>;
  using ProgressCb = std::function<void(int percent)>; // 0-100
  using ResultCb = std::function<void(bool success, const std::string &errmsg)>;

  virtual ~IFileSystemModel() = default;

  // 列出驱动器（例如 C:, D:）
  virtual void listDrives(DrivesCb cb) = 0;

  // 列出目录：采用流式回调，参数 hasMore 表示后续是否仍有数据
  virtual void listDirectory(const std::string &path, ListCb cb) = 0;

  // 下载/上传返回一个 TransferHandle，调用者可以通过 handle->cancel() 取消
  virtual std::shared_ptr<TransferHandle>
  downloadFile(const std::string &remotePath, const std::string &localPath,
               ProgressCb progress, ResultCb result) = 0;

  virtual std::shared_ptr<TransferHandle>
  uploadFile(const std::string &localPath, const std::string &remotePath,
             ProgressCb progress, ResultCb result) = 0;

  // 兼容性方法：按 transferId 取消（若需要）
  virtual void cancelTransfer(const std::string &transferId) = 0;

  // 删除与运行远端文件
  virtual void deleteFile(const std::string &path, ResultCb cb) = 0;
  virtual void runFile(const std::string &path, ResultCb cb) = 0;
};



// ---------------- IMonitorModel ----------------

class IMonitorModel : public IModel,
                      public NetworkProtocol,
                      public MonitorProtocol {
public:

  using FrameCb = std::function<void(std::shared_ptr<const FrameData>)>;
  virtual ~IMonitorModel() = default;

  virtual void startCapture(int fps, FrameCb cb) = 0;
  virtual void stopCapture() = 0;

  // 录制控制：将接收到的每帧保存到磁盘（Model 层实现），
  // dir 为目标保存目录（若为空则使用默认 recordings 目录）。
  virtual void startRecording(const std::string &dir) = 0;
  virtual void stopRecording() = 0;

  // 将 IIoModel 注入到 MonitorModel，使 Model 可在需要时委托输入注入。
  virtual void setIoModel(std::shared_ptr<IIoModel> io) = 0;
  
  // 将 INetworkModel 注入到 MonitorModel，使 Model 可以发送屏幕请求包。
  virtual void setNetworkModel(std::shared_ptr<INetworkModel> net) = 0;

  // 便捷转发接口（Controller 可直接通过 MonitorModel 调用，这些将委托给
  // IIoModel）
  virtual void injectMouse(int x, int y, int button, bool down) = 0;
  virtual void injectKey(int keycode, bool down) = 0;
};

// ---------------- IIoModel ----------------
// 输入注入接口，将键鼠注入职责从监视模型中解耦出来，便于替换和测试。
class IIoModel : public IModel, public NetworkProtocol {
public:
  virtual ~IIoModel() = default;

  // 坐标基于远端画布坐标系（0..width-1）
  virtual void injectMouse(int x, int y, int button, bool down) = 0;
  virtual void injectKey(int keycode, bool down) = 0;
  // 发送锁机/解锁命令到远端
  virtual void sendLockCommand(bool lock) = 0;
};
