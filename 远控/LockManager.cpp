#include "pch.h"
#include "LockManager.h"
#include "LockDialog.h"
#include "framework.h"
#include "Resource.h"
#include <process.h>
#include <atomic>

LockManager::LockManager() : m_hThread(NULL), m_threadId(0), m_locked(false) {}

LockManager::~LockManager() {
  if (IsLocked()) {
    Unlock();
  }
  if (m_hThread) {
    WaitForSingleObject(m_hThread, INFINITE);
    CloseHandle(m_hThread);
    m_hThread = NULL;
  }
}

int LockManager::Lock() {
  if (m_locked.load())
    return 0; // already locked

  unsigned tid = 0;
  m_hThread = (HANDLE)_beginthreadex(NULL, 0, ThreadFunc, NULL, 0, &tid);
  if (m_hThread == NULL) {
    return -1;
  }
  m_threadId = tid;
  m_locked.store(true);
  return 0;
}

int LockManager::Unlock() {
  if (!m_locked.load())
    return 0; // already unlocked

  if (m_threadId != 0) {
    PostThreadMessage(m_threadId, WM_KEYDOWN, 0, 0);
    if (m_hThread) {
      WaitForSingleObject(m_hThread, INFINITE);
      CloseHandle(m_hThread);
      m_hThread = NULL;
    }
    m_threadId = 0;
  }
  m_locked.store(false);
  return 0;
}

bool LockManager::IsLocked() const { return m_locked.load(); }

unsigned __stdcall LockManager::ThreadFunc(void *param) {
  // 在此线程内创建对话框并运行消息循环
  CLockDialog dlg;
  dlg.Create(IDD_DIALOG_INFO);
  dlg.ShowWindow(SW_SHOW);
  dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
  ShowCursor(false);
  CRect rect;
  dlg.GetWindowRect(&rect);
  ClipCursor(&rect);

  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    if (msg.message == WM_KEYDOWN) {
      break;
    }
  }
  ClipCursor(NULL);
  dlg.DestroyWindow();

  ShowCursor(true);
  _endthreadex(0);
  return 0;
}
