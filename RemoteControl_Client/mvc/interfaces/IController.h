#pragma once

#include "../model/Interface.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// Controller接口定义
// 职责: 协调Model和View之间的交互,处理业务逻辑
// ============================================================================

// ----------------------------------------------------------------------------
// IMainController: 主窗口Controller接口
// 协调 INetworkModel 和 IFileSystemModel,处理主窗口的用户操作
// ----------------------------------------------------------------------------
class IMainController {
public:
  virtual ~IMainController() = default;

  // ---- 连接管理 ----
  // 处理用户的连接请求
  virtual void OnConnectRequested(const std::string &ip, uint16_t port) = 0;

  // 处理用户的断开连接请求
  virtual void OnDisconnectRequested() = 0;

  // ---- 文件系统操作 ----
  // 刷新驱动器列表
  virtual void OnRefreshDrives() = 0;

  // 用户选择了某个目录(树控件选择变化)
  virtual void OnDirectorySelected(const std::string &path) = 0;

  // 用户双击了某个目录(展开子目录)
  virtual void OnDirectoryExpanded(const std::string &path) = 0;

  // ---- 文件操作 ----
  // 下载文件: remotePath为服务器路径, localPath为本地保存路径
  virtual void OnFileDownload(const std::string &remotePath,
                              const std::string &localPath) = 0;

  // 上传文件: localPath为本地文件路径, remotePath为服务器目标路径
  virtual void OnFileUpload(const std::string &localPath,
                            const std::string &remotePath) = 0;

  // 删除远程文件
  virtual void OnFileDelete(const std::string &path) = 0;

  // 运行远程文件
  virtual void OnFileRun(const std::string &path) = 0;

  // ---- 监视功能 ----
  // 启动屏幕监视(创建并显示监视窗口)
  virtual void OnStartMonitor() = 0;

  // 停止屏幕监视(关闭监视窗口并清理资源)
  virtual void OnStopMonitor() = 0;
};

// ----------------------------------------------------------------------------
// IMonitorController: 监视窗口Controller接口
// 协调 IMonitorModel 和 IIoModel,处理监视窗口的用户操作
// ----------------------------------------------------------------------------
class IMonitorController {
public:
  virtual ~IMonitorController() = default;

  // ---- 屏幕捕获控制 ----
  // 启动屏幕捕获: fps为帧率(1-60)
  virtual void OnStartCapture(int fps) = 0;

  // 停止屏幕捕获
  virtual void OnStopCapture() = 0;

  // ---- 录制控制 ----
  // 开始录制: dir为保存目录(空字符串使用默认目录)
  virtual void OnStartRecording(const std::string &dir = "") = 0;

  // 停止录制
  virtual void OnStopRecording() = 0;

  // ---- 输入注入 ----
  // 鼠标输入: x,y为画布坐标, button为按键(0=左键,1=右键,2=中键),
  // down为按下/释放
  virtual void OnMouseInput(int x, int y, int button, bool down) = 0;

  // 鼠标移动
  virtual void OnMouseMove(int x, int y) = 0;

  // 键盘输入: keycode为虚拟键码, down为按下/释放
  virtual void OnKeyInput(int keycode, bool down) = 0;

  // ---- 控制开关 ----
  // 启用/禁用鼠标控制
  virtual void OnMouseControlToggle(bool enabled) = 0;

  // 启用/禁用键盘控制
  virtual void OnKeyboardControlToggle(bool enabled) = 0;

  // ---- 截图功能 ----
  // 保存当前帧为图片: savePath为保存路径
  virtual void OnSaveScreenshot(const std::string &savePath) = 0;

  // ---- 缩放控制 ----
  // 设置缩放比例: scale为缩放倍数(0.5 = 50%, 1.0 = 100%, 2.0 = 200%)
  virtual void OnSetZoom(float scale) = 0;

  // 适应窗口大小
  virtual void OnFitToWindow() = 0;

  // 原始大小
  virtual void OnActualSize() = 0;

  // 锁定/解锁远程屏幕
  virtual void OnLockScreen() = 0;
  virtual void OnUnlockScreen() = 0;
};
