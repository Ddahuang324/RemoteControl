#pragma once
#include <thread>
#include <atomic>

class ScreenStreamer {
public:
  ScreenStreamer();
  ~ScreenStreamer();

  // 立即发送一次屏幕截图，返回 0 表示成功，负值表示失败
  int SendOnce();

  // 开始/停止屏幕流
  void StartStream(int intervalMs = 100);
  void StopStream();
  bool IsRunning() const;

private:
  void ThreadLoop(int intervalMs);

  std::thread m_thread;
  std::atomic<bool> m_running;
};
