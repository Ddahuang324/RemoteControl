#pragma once

#include "../../../Infra/Packet.hpp"
#include "../../../Infra/Socket.hpp"
#include "../../../Infra/ThreadPool.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// InterfaceProtocol: 资源容器，供 IModel 及其派生类使用
// 全局 FrameData 结构：用于在 Model/View 之间传递帧数据（RGBA 或 压缩载荷）
struct FrameData {
  std::vector<uint8_t> rgba;
  int width = 0;
  int height = 0;
  uint64_t timestampMs = 0;

  // 元信息
  // - isFullFrame: 标记此帧是否为完整帧（相对于增量/差分帧）
  bool isFullFrame = false;

  // 若为压缩载荷，则使用以下字段携带原始压缩数据与 mime 类型。
  // 当 compressed==true 时，解码器应读取 compressedPayload 并根据 mimeType
  // 解码到 rgba。
  bool compressed = false;
  std::vector<uint8_t> compressedPayload;
  std::string mimeType; // 例如 "image/png" 或其他自定义载荷类型

  // ROI（差异包）描述此帧在远端画布中的位置与大小。
  // 对于全屏帧通常为
  // (0,0,width,height)。对差异/分块帧，客户端应将该区域拷贝到目标画布对应位置。
  int roi_x = 0;
  int roi_y = 0;
  int roi_w = 0;
  int roi_h = 0;
};
// 前向声明：decoder 与 IIoModel 在实现端定义于全局命名空间
struct IDecoder; // decoder 接口可在实现端定义为全局类型以匹配此处
class IIoModel;  // IIoModel 在 Interface.h 中声明，前向声明以便在
                 // MonitorResources 中使用

struct NetworkProtocol {
  NetworkProtocol() {
    m_netRes = std::make_unique<NetworkResources>();
    m_netBuffer = std::make_unique<NetworkBuffer>();
  }
  virtual ~NetworkProtocol() = default;

  struct NetworkResources {
    std::unique_ptr<Socket> socket;
    std::thread recvThread;
    std::atomic_bool running{false};
    std::function<void(const Packet &)> packetCb;
    std::function<void(bool)> statusCb;
  };

  struct NetworkBuffer {
    std::deque<Packet> packetQueue;
    std::mutex queueMutex;
    std::condition_variable queueCv;
    size_t maxQueue = 8;
  };

  std::unique_ptr<NetworkResources> m_netRes;
  std::unique_ptr<NetworkBuffer> m_netBuffer;
};

struct MonitorProtocol {
  MonitorProtocol() {
    m_screenBuffer = std::make_unique<ScreenBuffer>();
    m_screenPacketData = std::make_unique<ScreenPacketData>();
    m_monitorRes = std::make_unique<MonitorResources>();
  }
  virtual ~MonitorProtocol() = default;

  struct ScreenPacketData {
    std::vector<uint8_t> rgba;
    int width = 0;
    int height = 0;
    uint64_t timestampMs = 0;
    bool isFullFrame = false;
    bool compressed = false;
    std::vector<uint8_t> compressedPayload;
    std::string mimeType;
    int roi_x = 0;
    int roi_y = 0;
    int roi_w = 0;
    int roi_h = 0;
  };

  struct ScreenBuffer {
    std::shared_ptr<ThreadPool> pool;
    std::deque<ScreenPacketData> screenPacketQueue;
    std::mutex screenQueueMutex;
    size_t maxScreenQueue = 4;
  };

  struct MonitorResources {
    std::atomic_bool running{false};
    std::mutex screenCvMutex;
    std::condition_variable screenCv;
    int fps = 10;
    std::function<void(std::shared_ptr<const ::FrameData>)> frameCb;
    std::shared_ptr<IDecoder> decoder;
    std::thread netThread;
    std::thread decodeThread;
    std::thread requestThread;
    std::vector<uint8_t> canvas;
    int canvasW = 0;
    int canvasH = 0;
    std::mutex canvasMutex;
    std::atomic<bool> recording{false};
    std::string recordDir;
    std::mutex recordMutex;
    std::atomic<int> recordFrameIndex{0};
    std::weak_ptr<IIoModel> ioModel;
  };

  std::unique_ptr<MonitorResources> m_monitorRes;
  std::unique_ptr<ScreenBuffer> m_screenBuffer;
  std::unique_ptr<ScreenPacketData> m_screenPacketData;
};

// FileSystemProtocol: 文件系统相关资源容器，负责管理文件传输生命周期
// 设计原则：将文件传输所需的线程池、包队列、传输注册表等集中在此处，
// 由 FileSystemModel（或其他需要的 Model）按需继承或组合使用。
struct FileSystemProtocol {
  FileSystemProtocol() {
    m_fileBuffer = std::make_unique<FileBuffer>();
    m_fileResources = std::make_unique<FileResources>();
    m_filePacketData = std::make_unique<FilePacketData>();
  }
  virtual ~FileSystemProtocol() = default;

  struct FilePacketData {
    std::vector<uint8_t> data;
    size_t offset = 0;
    uint64_t timestampMs = 0;
  };

  struct FileBuffer {
    std::shared_ptr<ThreadPool> pool;
    std::deque<Packet> filePacketQueue;
    std::mutex fileQueueMutex;
    size_t maxFileQueue = 16;
  };

  struct FileResources {
    // 传输注册表: key = transferId, value = cancelled flag
    std::map<std::string, bool> activeTransfers;
    std::mutex transfersMutex;

    // 用于保护文件写入/读写相关临界区
    std::mutex fileIoMutex;
  };

  // 统一的文件条目定义：协议层持有资源类型，供上层 Model/Controller/View 重用
  struct FileEntry {
    std::string fullPath;
    std::string name;
    bool isDirectory = false;
    uint64_t size = 0;
    uint64_t mtime = 0; 
  };

  std::unique_ptr<FileResources> m_fileResources;
  std::unique_ptr<FileBuffer> m_fileBuffer;
  std::unique_ptr<FilePacketData> m_filePacketData;
};
