#include "pch.h"
#include "network/ServerSocket.h"
#include "core/Enities.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

// 包含锁机相关
#include "../RemoteControl_Server/include/core/Enities.h"
#include "../RemoteControl_Server/include/ui/LockDialog.h"

// 外部函数声明
extern void LockMachine(class CServerSocket& serverSocket, const Cpacket& packet);
extern void UnlockMachine(class CServerSocket& serverSocket, const Cpacket& packet);
extern bool IsMachineLocked();
extern DWORD GetLockThreadId();
extern bool IsLockDialogCreated();
extern void ResetLockState();

// Mock CServerSocket for testing
class MockServerSocket : public CServerSocket {
public:
    MockServerSocket() : CServerSocket(0) {}  // dummy port
    std::vector<Cpacket> sentPackets;
    void SendPacket(const Cpacket& packet) override {
        sentPackets.push_back(packet);
    }
};

// Helper function to wait for lock state
void WaitForLockState(bool expectedState, int timeoutMs = 500) {
    auto start = std::chrono::steady_clock::now();
    while (IsMachineLocked() != expectedState) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > timeoutMs) {
            break;
        }
    }
}

// 测试锁机功能
TEST(LockMachineTest, LocksMachineWhenNotLocked) {
    ResetLockState();

    MockServerSocket mockSocket;
    Cpacket lockPacket(CMD_LOCK_MACHINE, {});

    // 调用锁机函数
    LockMachine(mockSocket, lockPacket);

    // 等待锁机状态
    WaitForLockState(true);

    // 检查状态：应该已锁定，且有线程ID
    EXPECT_TRUE(IsMachineLocked());
    EXPECT_NE(GetLockThreadId(), 0);

    // 检查UI是否创建
    EXPECT_TRUE(IsLockDialogCreated());

    // 检查发送的确认包
    ASSERT_EQ(mockSocket.sentPackets.size(), 1);
    EXPECT_EQ(mockSocket.sentPackets[0].sCmd, CMD_LOCK_MACHINE);
}

// 测试重复锁机（应该忽略）
TEST(LockMachineTest, IgnoresLockWhenAlreadyLocked) {
    ResetLockState();

    MockServerSocket mockSocket;
    Cpacket lockPacket(CMD_LOCK_MACHINE, {});

    // 第一次锁机
    LockMachine(mockSocket, lockPacket);
    WaitForLockState(true);

    // 第二次锁机
    MockServerSocket mockSocket2;
    LockMachine(mockSocket2, lockPacket);

    // 应该只有一个包发送（第一次）
    EXPECT_EQ(mockSocket.sentPackets.size(), 1);
    EXPECT_EQ(mockSocket2.sentPackets.size(), 0);  // 第二次不发送
}

// 测试解锁功能
TEST(UnlockMachineTest, UnlocksMachineWhenLocked) {
    ResetLockState();

    MockServerSocket mockSocket;
    Cpacket lockPacket(CMD_LOCK_MACHINE, {});
    LockMachine(mockSocket, lockPacket);
    WaitForLockState(true);

    MockServerSocket unlockSocket;
    Cpacket unlockPacket(CMD_UNLOCK_MACHINE, {});
    UnlockMachine(unlockSocket, unlockPacket);

    // 等待解锁状态
    WaitForLockState(false);

    // 检查状态：应该已解锁
    EXPECT_FALSE(IsMachineLocked());
    EXPECT_EQ(GetLockThreadId(), 0);

    // 检查发送的确认包
    ASSERT_EQ(unlockSocket.sentPackets.size(), 1);
    EXPECT_EQ(unlockSocket.sentPackets[0].sCmd, CMD_UNLOCK_MACHINE);
}

// 测试解锁未锁定的机器（应该忽略）
TEST(UnlockMachineTest, IgnoresUnlockWhenNotLocked) {
    ResetLockState();

    MockServerSocket mockSocket;
    Cpacket unlockPacket(CMD_UNLOCK_MACHINE, {});

    // 调用解锁
    UnlockMachine(mockSocket, unlockPacket);

    // 状态应该不变
    EXPECT_FALSE(IsMachineLocked());
    EXPECT_EQ(GetLockThreadId(), 0);

    // 不应该发送包
    EXPECT_EQ(mockSocket.sentPackets.size(), 0);
}

// 添加锁机功能的单元测试
TEST(LockMachineTest, LockAndUnlock) {
    ResetLockState();

    MockServerSocket mockSocket;
    Cpacket lockPacket(CMD_LOCK_MACHINE, {});

    // 测试锁机
    LockMachine(mockSocket, lockPacket);
    WaitForLockState(true);
    EXPECT_TRUE(IsMachineLocked());

    // 测试解锁
    Cpacket unlockPacket(CMD_UNLOCK_MACHINE, {});
    UnlockMachine(mockSocket, unlockPacket);
    WaitForLockState(false);
    EXPECT_FALSE(IsMachineLocked());
}

int main(int argc, char** argv) {
    // 添加诊断日志
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "[MAIN] RCTest.exe 启动" << std::endl;
    std::cout << "[MAIN] 命令行参数个数: " << argc << std::endl;
    for (int i = 0; i < argc; ++i) {
        std::cout << "[MAIN] argv[" << i << "] = " << argv[i] << std::endl;
    }
    std::cout << std::string(80, '=') << std::endl << std::endl;

    // 正常执行 Google Test
    std::cout << "[MAIN] 执行 Google Test 单元测试" << std::endl;
    ::testing::InitGoogleTest(&argc, argv);
    
    // 列出所有可用的测试
    std::cout << "\n[MAIN] 所有注册的测试:" << std::endl;
    std::cout << "[MAIN] 提示：要运行特定测试，使用 --gtest_filter=TestName" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    int result = RUN_ALL_TESTS();

    // Final cleanup to ensure the process exits cleanly
    ResetLockState(); 
    
    return result;
}
