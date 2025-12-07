#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <winsock2.h>

using BYTE = unsigned char;

// 发送优先级定义
enum class SendPriority {
  HIGH = 0,   // 鼠标/键盘事件 - 最高优先级
  NORMAL = 1, // 普通命令（文件操作、ACK等）
  LOW = 2     // 屏幕数据帧 - 最低优先级
};

// 待发送数据项
struct SendItem {
  std::vector<BYTE> data; // 已序列化的数据
  SendPriority priority;  // 优先级
  uint64_t sequence;      // 入队序号，用于同优先级FIFO排序

  // 优先级队列比较器：priority小的优先，同priority按sequence小的优先
  bool operator>(const SendItem &other) const {
    if (priority != other.priority) {
      return static_cast<int>(priority) > static_cast<int>(other.priority);
    }
    return sequence > other.sequence;
  }
};

// 优先级发送队列
class SendQueue {
public:
  // 构造函数：传入socket引用和可选的低优先级队列容量限制
  explicit SendQueue(SOCKET &clientSocket, size_t lowPriorityMaxSize = 10)
      : m_clientSocket(clientSocket), m_lowPriorityMaxSize(lowPriorityMaxSize),
        m_running(false), m_sequenceCounter(0), m_lowPriorityCount(0) {}

  ~SendQueue() { Stop(); }

  // 启动发送线程
  void Start() {
    if (m_running.exchange(true)) {
      return; // 已经在运行
    }
    m_sendThread = std::thread(&SendQueue::SendThreadFunc, this);
  }

  // 停止发送线程
  void Stop() {
    if (!m_running.exchange(false)) {
      return; // 已经停止
    }
    m_cv.notify_all();
    if (m_sendThread.joinable()) {
      m_sendThread.join();
    }
    // 清空队列
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_queue.empty()) {
      m_queue.pop();
    }
    m_lowPriorityCount = 0;
  }

  // 入队操作 - 线程安全，极快返回
  void Enqueue(const std::vector<BYTE> &data,
               SendPriority priority = SendPriority::NORMAL) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 低优先级队列容量控制：丢弃旧帧防止内存溢出
    if (priority == SendPriority::LOW) {
      if (m_lowPriorityCount >= m_lowPriorityMaxSize) {
        // 找到并移除最旧的低优先级项（通过重建队列）
        std::priority_queue<SendItem, std::vector<SendItem>,
                            std::greater<SendItem>>
            newQueue;
        bool removedOne = false;
        uint64_t oldestLowSeq = UINT64_MAX;

        // 先找到最旧的低优先级项的sequence
        std::vector<SendItem> items;
        while (!m_queue.empty()) {
          items.push_back(m_queue.top());
          m_queue.pop();
        }

        // 找到最旧的低优先级sequence
        for (const auto &item : items) {
          if (item.priority == SendPriority::LOW &&
              item.sequence < oldestLowSeq) {
            oldestLowSeq = item.sequence;
          }
        }

        // 重建队列，跳过最旧的低优先级项
        for (auto &item : items) {
          if (!removedOne && item.priority == SendPriority::LOW &&
              item.sequence == oldestLowSeq) {
            removedOne = true;
            m_lowPriorityCount--;
            continue; // 丢弃这一项
          }
          m_queue.push(std::move(item));
        }

        std::cout << "[SendQueue] Low priority queue full, dropped oldest frame"
                  << std::endl;
      }
      m_lowPriorityCount++;
    }

    SendItem item;
    item.data = data;
    item.priority = priority;
    item.sequence = m_sequenceCounter++;

    m_queue.push(std::move(item));
    m_cv.notify_one();
  }

  // 入队操作 - 移动语义版本
  void Enqueue(std::vector<BYTE> &&data,
               SendPriority priority = SendPriority::NORMAL) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 低优先级队列容量控制（同上）
    if (priority == SendPriority::LOW) {
      if (m_lowPriorityCount >= m_lowPriorityMaxSize) {
        std::vector<SendItem> items;
        while (!m_queue.empty()) {
          items.push_back(m_queue.top());
          m_queue.pop();
        }

        uint64_t oldestLowSeq = UINT64_MAX;
        for (const auto &item : items) {
          if (item.priority == SendPriority::LOW &&
              item.sequence < oldestLowSeq) {
            oldestLowSeq = item.sequence;
          }
        }

        bool removedOne = false;
        for (auto &item : items) {
          if (!removedOne && item.priority == SendPriority::LOW &&
              item.sequence == oldestLowSeq) {
            removedOne = true;
            m_lowPriorityCount--;
            continue;
          }
          m_queue.push(std::move(item));
        }

        std::cout << "[SendQueue] Low priority queue full, dropped oldest frame"
                  << std::endl;
      }
      m_lowPriorityCount++;
    }

    SendItem item;
    item.data = std::move(data);
    item.priority = priority;
    item.sequence = m_sequenceCounter++;

    m_queue.push(std::move(item));
    m_cv.notify_one();
  }

  // 检查队列是否为空
  bool Empty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.empty();
  }

  // 获取队列大小
  size_t Size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
  }

private:
  // 检查队列中是否有高优先级数据等待
  bool HasHighPriorityPending() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty())
      return false;
    return m_queue.top().priority == SendPriority::HIGH;
  }

  // 发送线程函数
  void SendThreadFunc() {
    std::cout << "[SendQueue] Send thread started" << std::endl;

    while (m_running) {
      // 1.
      // 检查是否有高优先级数据需要优先处理（即使有暂存数据也要先处理高优先级）
      if (!m_pendingBuffer.empty() && !HasHighPriorityPending()) {
        // 没有高优先级数据时才恢复发送暂存数据
        if (DoSendWithPreemption(m_pendingBuffer, m_pendingOffset)) {
          m_pendingBuffer.clear();
          m_pendingOffset = 0;
        }
        continue; // 重新检查队列
      }

      // 2. 从队列取出一项
      SendItem item;
      {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || !m_running; });

        if (!m_running && m_queue.empty()) {
          break;
        }

        if (m_queue.empty()) {
          continue;
        }

        item = std::move(const_cast<SendItem &>(m_queue.top()));
        m_queue.pop();

        if (item.priority == SendPriority::LOW) {
          m_lowPriorityCount--;
        }
      }

      // 3. 发送（可能被抢占）
      DoSendWithPreemption(item.data, 0);
    }

    std::cout << "[SendQueue] Send thread stopped" << std::endl;
  }

  // 分块发送，支持高优先级抢占
  // 返回值：true=发送完成，false=被抢占需暂存
  bool DoSendWithPreemption(const std::vector<BYTE> &buffer,
                            size_t startOffset = 0) {
    if (m_clientSocket == INVALID_SOCKET) {
      return true;
    }

    const char *ptr =
        reinterpret_cast<const char *>(buffer.data()) + startOffset;
    size_t remaining = buffer.size() - startOffset;
    const size_t CHUNK_SIZE =
        4 * 1024; // 4KB chunks - 最快响应抢占，约 0.32ms @ 100Mbps

    while (remaining > 0 && m_running) {
      // 发送一块
      size_t chunkSize = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
      int sendSize = static_cast<int>(chunkSize);
      int sent = send(m_clientSocket, ptr, sendSize, 0);

      if (sent == SOCKET_ERROR) {
        std::cerr << "[SendQueue] send failed with error: " << WSAGetLastError()
                  << std::endl;
        return true; // 出错视为完成
      }

      ptr += sent;
      remaining -= sent;

      // 检查是否有高优先级数据需要抢占（仅在还有剩余数据时）
      if (remaining > 0 && HasHighPriorityPending()) {
        std::cout << "[SendQueue] HIGH priority preemption triggered, "
                  << remaining << " bytes pending" << std::endl;
        // 保存剩余数据到暂存区
        size_t sentBytes = buffer.size() - startOffset - remaining;
        m_pendingBuffer = buffer;
        m_pendingOffset = startOffset + sentBytes;
        return false; // 被抢占
      }
    }
    return true; // 发送完成
  }

  // 禁止拷贝
  SendQueue(const SendQueue &) = delete;
  SendQueue &operator=(const SendQueue &) = delete;

private:
  SOCKET &m_clientSocket;                  // 客户端socket引用
  size_t m_lowPriorityMaxSize;             // 低优先级队列最大容量
  std::atomic<bool> m_running;             // 运行状态
  std::atomic<uint64_t> m_sequenceCounter; // 序列号计数器
  std::atomic<size_t> m_lowPriorityCount;  // 低优先级项计数

  mutable std::mutex m_mutex;   // 队列锁
  std::condition_variable m_cv; // 条件变量
  std::thread m_sendThread;     // 发送线程

  // 优先级队列：使用std::greater使小值优先
  std::priority_queue<SendItem, std::vector<SendItem>, std::greater<SendItem>>
      m_queue;

  // 被高优先级抢占而暂存的待续发送数据
  std::vector<BYTE> m_pendingBuffer;
  size_t m_pendingOffset = 0;
};
