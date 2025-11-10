#include "pch.h"
#include "LockMachine.h"
#include "../../include/Resource.h"
#include <mutex>
#include <condition_variable>
#include <iostream> // For std::cout/cerr

// Global state variables for lock management
static std::atomic<bool> g_isLocked = false;
static std::atomic<DWORD> g_lockThreadId = 0;
static CLockDialog* g_lockDialog = nullptr;
static std::mutex g_lockMutex; // Protects access to g_lockDialog and related state changes

#define WM_APP_UNLOCK_MACHINE (WM_APP + 1)

// Thread function to create and manage the lock UI
UINT LockUIThread(LPVOID pParam) {
    CWinThread* pThread = AfxGetThread();
    if (!pThread) return 1;

    g_lockThreadId = GetCurrentThreadId();
    std::cout << "LockUIThread: Started with ThreadId: " << g_lockThreadId << std::endl;

    MSG msg;

    try {
        // Create and show the lock dialog
        {
            std::lock_guard<std::mutex> lock(g_lockMutex);
            if (g_lockDialog) { // Should not happen, but as a safeguard
                delete g_lockDialog;
            }
            g_lockDialog = new CLockDialog();
            if (!g_lockDialog->Create(IDD_DIALOG_INFO)) {
                std::cerr << "LockUIThread: Failed to create Lock Dialog." << std::endl;
                delete g_lockDialog;
                g_lockDialog = nullptr;
                g_isLocked = false;
                g_lockThreadId = 0;
                return 1; // Creation failed
            }
        }

        g_lockDialog->ShowWindow(SW_SHOWMAXIMIZED); // Show maximized to cover the screen
        ShowCursor(FALSE);

        // Message loop for the UI thread
        while (GetMessage(&msg, NULL, 0, 0)) {
            if (msg.message == WM_APP_UNLOCK_MACHINE) {
                std::cout << "LockUIThread: Received WM_APP_UNLOCK_MACHINE. Exiting." << std::endl;
                break; // Exit signal
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

    } catch (CException* e) {
        TCHAR errorMsg[256];
        if (e->GetErrorMessage(errorMsg, 256)) {
            std::cerr << "LockUIThread: MFC Exception: " << errorMsg << std::endl;
        } else {
            std::cerr << "LockUIThread: MFC Exception occurred, but no error message available." << std::endl;
        }
        e->Delete();
    } catch (...) {
        std::cerr << "LockUIThread: Unknown exception." << std::endl;
    }

    // Cleanup
    std::cout << "LockUIThread: Cleaning up and exiting." << std::endl;
    ShowCursor(TRUE);
    
    {
        std::lock_guard<std::mutex> lock(g_lockMutex);
        if (g_lockDialog) {
            if (g_lockDialog->GetSafeHwnd()) {
                g_lockDialog->DestroyWindow();
            }
            delete g_lockDialog;
            g_lockDialog = nullptr;
        }
        g_isLocked = false;
        g_lockThreadId = 0;
    }

    return 0; // Thread finished successfully
}

// Function to lock the machine
void LockMachine(CServerSocket& serverSocket, const Cpacket& packet) {
    if (g_isLocked) {
        std::cout << "LockMachine: Machine is already locked." << std::endl;
        return;
    }
    std::cout << "LockMachine: Attempting to lock machine..." << std::endl;

    g_isLocked = true; // Set lock state early to prevent race conditions

    // Use AfxBeginThread to create an MFC-compatible UI thread
    CWinThread* pThread = AfxBeginThread(LockUIThread, nullptr);
    if (!pThread) {
        std::cerr << "LockMachine: Failed to create LockUIThread using AfxBeginThread." << std::endl;
        g_isLocked = false; // Rollback state if thread creation fails
        return;
    }

    // Wait until the thread has started and set its ID
    while (g_lockThreadId == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "LockMachine: Lock thread created successfully." << std::endl;
    
    // Send confirmation packet
    Cpacket response(CMD_LOCK_MACHINE, {});
    serverSocket.SendPacket(response);
}

// Function to unlock the machine
void UnlockMachine(CServerSocket& serverSocket, const Cpacket& packet) {
    DWORD threadId = g_lockThreadId.load();
    if (!g_isLocked || threadId == 0) {
        std::cout << "UnlockMachine: Machine is not locked or thread ID is 0." << std::endl;
        return;
    }

    std::cout << "UnlockMachine: Posting WM_APP_UNLOCK_MACHINE to thread " << threadId << std::endl;
    if (!PostThreadMessage(threadId, WM_APP_UNLOCK_MACHINE, 0, 0)) {
        std::cerr << "UnlockMachine: Failed to post message. Forcing cleanup." << std::endl;
        // Force cleanup if posting message fails
        ResetLockState();
    }

    // Wait for the lock to be released
    while (g_isLocked) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    std::cout << "UnlockMachine: Machine successfully unlocked." << std::endl;

    // Send confirmation packet
    Cpacket response(CMD_UNLOCK_MACHINE, {});
    serverSocket.SendPacket(response);
}

// Helper function to check if the machine is locked
bool IsMachineLocked() {
    return g_isLocked.load();
}

// Helper function to get the lock thread ID
DWORD GetLockThreadId() {
    return g_lockThreadId.load();
}

// Helper function to check if the lock dialog is created
bool IsLockDialogCreated() {
    std::lock_guard<std::mutex> lock(g_lockMutex);
    return g_lockDialog != nullptr && g_lockDialog->GetSafeHwnd() != NULL;
}

// Function to reset the state, primarily for testing
void ResetLockState() {
    std::cout << "ResetLockState: Attempting to reset lock state..." << std::endl;
    DWORD threadId = g_lockThreadId.load();
    if (threadId != 0) {
        std::cout << "ResetLockState: Found active lock thread " << threadId << ". Sending unlock message." << std::endl;
        PostThreadMessage(threadId, WM_APP_UNLOCK_MACHINE, 0, 0);
        
        // Wait for the thread to terminate and clean up
        int timeout = 100; // 1 second timeout
        while (g_isLocked.load() && timeout > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            timeout--;
        }
        if (g_isLocked.load()) {
             std::cerr << "ResetLockState: Timed out waiting for thread to exit. State may be inconsistent." << std::endl;
        }
    }

    // Force reset global state as a final measure
    g_isLocked = false;
    g_lockThreadId = 0;
    {
        std::lock_guard<std::mutex> lock(g_lockMutex);
        if (g_lockDialog) {
            if (g_lockDialog->GetSafeHwnd()) {
                g_lockDialog->DestroyWindow();
            }
            delete g_lockDialog;
            g_lockDialog = nullptr;
        }
    }
    std::cout << "ResetLockState: State has been forcefully reset." << std::endl;
}