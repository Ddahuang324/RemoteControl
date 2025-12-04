#pragma once

// 注意：仓库中存在拼写为 `MoniterModel.h` 的历史文件名（拼写错误）。
// 接口定义已放在 `Interface.h` 中，请使用 `IMonitorModel`。
#include "Interface.h"
#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <mutex>

// 说明：保留该文件以兼容当前工程；后续建议重命名为 `MonitorModel.h`
// 并统一引用。

class ThreadPool;

// 注意：保留旧拼写 Moniter，实际类命名为 MonitorModel
class MonitorModel : public IMonitorModel {
public:
  explicit MonitorModel(std::shared_ptr<ThreadPool> pool = nullptr);
  ~MonitorModel() override;

  void startCapture(int fps, FrameCb cb) override;
  void stopCapture() override;

  // 录制控制（Model 层实现）
  void startRecording(const std::string &dir) override;
  void stopRecording() override;

  // IIoModel 委托（Controller 可注入）
  void setIoModel(std::shared_ptr<IIoModel> io) override;
  
  // INetworkModel 注入（用于发送屏幕请求）
  void setNetworkModel(std::shared_ptr<INetworkModel> net) override;
  
  void injectMouse(int x, int y, int button, bool down) override;
  void injectKey(int keycode, bool down) override;

private:
  std::shared_ptr<INetworkModel> net_;  // 用于发送屏幕请求
};
