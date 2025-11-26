#pragma once
#include <windows.h>
#include <atomic>

class LockManager {
public:
  LockManager();
  ~LockManager();

  // 返回 0 表示成功，负值表示错误
  int Lock();
  int Unlock();
  bool IsLocked() const;

private:
  static unsigned __stdcall ThreadFunc(void *param);

  HANDLE m_hThread;
  unsigned m_threadId;
  std::atomic<bool> m_locked;
};
