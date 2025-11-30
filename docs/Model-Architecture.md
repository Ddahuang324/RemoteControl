# Model 层架构设计说明（中文）

## 目的与范围 ✅

本文档针对 `RemoteControl` 项目的 MVC 架构的 Model 层（`mvc/model/` 文件夹）设计说明与约束，目标是：

- 明确 Model 层的责任与边界。
- 定义 Model 的公共接口风格与线程/并发策略。
- 提供 Model 子模块示例（Network、FileSystem、Monitor）和交互流程（Controller -> Model -> View）。
- 提供测试与扩展建议，便于后续逐步实现与集成。

> 说明：当前仓库中已有的 Model 文件（如 `MoniterModel.h`、`NetworkModel.h` 等）多数为草稿或空文件，本设计仅新增文档，不对现有源文件做修改（符合“0改动，只添加”的原则）。

---

## 总体设计原则 💡

1. 单一职责：每个 Model 负责一类业务（网络、文件系统、监视/屏幕等）。
2. 依赖注入（DI）与接口编程：Controller 通过接口指向 Model（抽象基类或纯虚类），便于替换/Mock。
3. 最小暴露：Model 只暴露状态与行为接口，不应直接与 UI/Controller 的具体实现耦合。
4. 线程安全：Model 应自行保证多线程访问安全或通过约定（只在某个工作线程中调用）提供线程隔离。
5. 明显的回调/事件：Model 使用回调/观察者或未来/Promise 返回异步结果（避免直接阻塞调用线程）。
6. 配置与持久化：Model 负责自己的状态保存/恢复（如需要），但以简单的配置文件（JSON）为优先实现方式。

---

## 文件与目录布局建议 🔧

在 `RemoteControl_Client/mvc/model/` 下，建议按子模块组织：

- `INetworkModel.h`（接口）
- `NetworkModel.h` / `NetworkModel.cpp`（实现）
- `IFileSystemModel.h`
- `FileSystemModel.h` / `FileSystemModel.cpp`
- `IMonitorModel.h`
- `MonitorModel.h` / `MonitorModel.cpp`（或 `MoniterModel.*` 现有命名保留，后续统一更名）
- `models_common/`（可选：线程队列、日志、通用结构体）

命名规则：
- 接口前缀 `I`（如 `INetworkModel`）
- 实现名与接口同名去掉前缀（如 `NetworkModel`）
- 文件名使用 `PascalCase`（例如 `NetworkModel.h`）

---

## Model 接口设计示例（草案） 🧭

下面给出建议的接口样式（C++11/17），以便 Controller 与 Model 解耦：

### IModel 基类（可选）

```cpp
#pragma once

class IModel {
public:
    virtual ~IModel() = default;
};
```

### INetworkModel（示例）

职责：建立/断开连接、发送/接收消息、连接状态通知。

```cpp
#pragma once

#include <string>
#include <functional>

struct NetworkMessage {
    std::vector<uint8_t> payload;
    // metadata: type, id, timestamp
};

class INetworkModel : public IModel {
public:
    using OnDataReceived = std::function<void(const NetworkMessage&)>;
    using OnStatusChanged = std::function<void(bool connected)>;

    virtual ~INetworkModel() = default;

    virtual bool connect(const std::string& host, uint16_t port) = 0;
    virtual void disconnect() = 0;
    virtual bool send(const NetworkMessage& msg) = 0;

    // 注册回调
    virtual void setOnDataReceived(OnDataReceived cb) = 0;
    virtual void setOnStatusChanged(OnStatusChanged cb) = 0;
};
```

实现细节建议：`NetworkModel` 在内部维护一个 I/O 线程或使用现有的 `ClientSocket`，并保证回调在约定的线程上触发（比如 Controller 的主线程或 UI 线程或通过事件队列切换）。


### IFileSystemModel（示例）

职责：列目录、文件上传/下载、删除、进度回调。

```cpp
#pragma once

#include <string>
#include <functional>

class IFileSystemModel : public IModel {
public:
    using ProgressCb = std::function<void(int percent)>;
    using ResultCb = std::function<void(bool success, const std::string& errmsg)>;

    virtual ~IFileSystemModel() = default;

    virtual void listDirectory(const std::string& path, std::function<void(const std::vector<std::string>& files)> cb) = 0;
    virtual void downloadFile(const std::string& remotePath, const std::string& localPath, ProgressCb p, ResultCb r) = 0;
    virtual void uploadFile(const std::string& localPath, const std::string& remotePath, ProgressCb p, ResultCb r) = 0;
};
```

实现细节建议：文件传输应使用一个或多个工作线程，避免阻塞主线程；进度回调与最终结果回调需在同一线程/约定线程中调用，便于 Controller 处理。


### IMonitorModel（示例）

职责：负责屏幕/桌面帧采集，远程输入注入，性能监控数据采集。

```cpp
#pragma once

#include <functional>
#include <vector>

struct FrameData {
    // 若项目使用 OpenCV，可直接使用 cv::Mat
    std::vector<uint8_t> rgba; // or other format
    int width;
    int height;
    uint64_t timestampMs;
};

class IMonitorModel : public IModel {
public:
    using FrameCb = std::function<void(const FrameData&)>;
    virtual ~IMonitorModel() = default;

    virtual void startCapture(int fps, FrameCb frameCb) = 0;
    virtual void stopCapture() = 0;

    // 可扩展：注入远控输入
    // virtual void injectMouse(int x, int y, int button, bool down) = 0;
    // virtual void injectKey(int keycode, bool down) = 0;
};
```

---

## 线程与同步（关键点） ⚙️

- Model 层实现建议在内部管理线程与资源，而对外暴露纯 API：Controller 调用异步方法即可，Model 做排队与处理。
- 推荐使用 `std::thread` + `std::mutex` + `std::condition_variable` 或 `std::async` 在 C++11/17 环境中实现工作队列。
- 所有回调建议在调用者约定的线程（UI 线程或 Controller 管理线程）触发。
- 使用 `std::atomic` 与最小粒度的锁（`std::lock_guard`）来降低锁争用。
- 对于大数据（如一帧图像），建议使用移动语义（`std::move`）或共享指针（`std::shared_ptr<const FrameData>`）来降低拷贝成本。

---

## 错误处理与日志 📛

- API 返回尽量使用 `bool` + `ResultCb` 或 `std::future<Result>`；或者在函数内抛出异常并由 Controller 捕获，具体风格须统一。
- 记录错误日志（建议使用一个简单的日志接口，或者临时方案使用 `OutputDebugString` 控制台打印）。

---

## 测试建议（单元/集成） ✅

- 使用 `gtest`（仓库已有 googletest）为每个 Model 提供单元测试；模拟网络/文件行为，验证回调、错误、重连、并发。
- 将 Mock 对象放在 `tests/mock/`，以便 Controller 与 Model 的集成测试。
- 目录 `RCTest/` 下可编写 Model 测试用例（例如网络连接断开重连、下载中断、帧率校验）。

---

## 示例：文件下载交互序列（Controller -> Model）

1. Controller 调用 `fileModel->downloadFile(remote, local, progressCb, resultCb);`。
2. `FileSystemModel` 将任务推入内部工作队列并立即返回（异步）。
3. 工作线程处理下载过程并在进度更新时调用 `progressCb(percent)`（回调在约定线程中触发）。
4. 下载成功/失败在完成时调用 `resultCb(success, errMsg)`。Controller 在回调中更新 View/UI。

---

## 兼容性与命名建议（关于 Moniter/Monitor） ⚠️

仓库中存在 `MoniterModel.h` 文件（拼写 Moniter），建议：
- 目前不更改文件名（遵守“0改动”原则）。
- 新的 Model 接口与实现使用统一命名 `MonitorModel`；并在后续可提出 PR 进行重命名与迁移（请确保所有引用同步更新）。

---

## 扩展点与未来工作 🛠️

- 为通信协议设计通用消息协议与序列化（JSON / protobuf），统一业务层格式。
- 提取通用组件：线程池、工作队列、写日志的公共库。
- 支持跨平台的屏幕捕获或设备抽象（当前使用 Windows API/DirectX/GDI+ 或 OpenCV 作为可选实现）。
- 提供一个 `ModelFactory` 或 `ModelManager`，在程序启动时根据配置选择不同实现（本地/模拟/测试/远控）。

---

## 结论及下一步建议 📌

- 我们已定义 Model 层的职责、接口风格、线程策略和测试建议。下一步可以逐个实现接口的最小可用实现（Skeleton），并逐步补齐功能与单元测试：
  1. NetworkModel：完整的连接/断开/收发接口并实现回调机制。
  2. FileSystemModel：支持上传/下载、列目录与进度回调。
  3. MonitorModel：支持帧采集并回调（使用 OpenCV 或系统 API）。

- 在 Controller 层引用 Model 接口，使用依赖注入（注入实现或 Mock）来进行集成测试。

> 如果你愿意，我可以继续：
> - 基于本设计创建接口的头文件草稿（只新增文件）并放到 `mvc/model` 下；
> - 或者分步骤实现 Network/FileSystem/Monitor 的基础骨架实现并添加单元测试。

---

文档作者：RemoteControl 项目架构设计（生成于 2025年11月）
