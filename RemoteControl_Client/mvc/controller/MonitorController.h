#pragma once

#include <string>
#include <memory>
#include <Windows.h>
#include "../model/Interface.h"
#include "../controller/IController.h"

class MonitorController : public IMonitorController {
public:
    explicit MonitorController(std::shared_ptr<IMonitorModel> model = nullptr);
    void startMonitoring();
    void stopMonitoring();
    void setModel(std::shared_ptr<IMonitorModel> model);
    void setIoModel(std::shared_ptr<IIoModel> io);
    // IMonitorController interface
    void OnStartCapture(int fps) override;
    void OnStopCapture() override;
    void OnStartRecording(const std::string &dir = "") override;
    void OnStopRecording() override;
    void OnMouseInput(int x, int y, int button, bool down) override;
    void OnMouseMove(int x, int y) override;
    void OnKeyInput(int keycode, bool down) override;
    void OnMouseControlToggle(bool enabled) override;
    void OnKeyboardControlToggle(bool enabled) override;
    void OnSaveScreenshot(const std::string &savePath) override;
    void OnSetZoom(float scale) override;
    void OnFitToWindow() override;
    void OnActualSize() override;
    void OnLockScreen() override;
    void OnUnlockScreen() override;
    // 将 View 的 HWND 注册到 Controller，Controller 会将 Model 的帧回调
    // 转发到该 HWND（通过 PostMessage）。可以传入 nullptr 来注销。
    void attachView(HWND hwnd);

private:
    std::shared_ptr<IMonitorModel> model_;
    std::shared_ptr<IIoModel> io_;
    HWND viewHwnd_ = nullptr;
};