#include "pch.h"
#include "MonitorController.h"
#include <iostream>

MonitorController::MonitorController(std::shared_ptr<IMonitorModel> model)
    : model_(std::move(model)) {}

void MonitorController::attachView(HWND hwnd) {
    viewHwnd_ = hwnd;
}

void MonitorController::setModel(std::shared_ptr<IMonitorModel> model) {
    model_ = std::move(model);
}

void MonitorController::setIoModel(std::shared_ptr<IIoModel> io) {
    io_ = std::move(io);
    if (model_ && io_) {
        model_->setIoModel(io_);
    }
}

// 启动监视功能
void MonitorController::startMonitoring() {
    if (!model_) {
        std::cout << "MonitorController: no model set" << std::endl;
        return;
    }
    // 默认 fps = 10
    model_->startCapture(10, [this](std::shared_ptr<const FrameData> frame) {
        if (!frame) return;
        if (viewHwnd_) {
            // 在 heap 上分配一个 shared_ptr 的副本并通过 PostMessage 传递指针。
            // View 收到消息后负责 delete 该指针并持有 shared_ptr 的副本。
            auto p = new std::shared_ptr<const FrameData>(frame);
            ::PostMessage(viewHwnd_, WM_APP + 0x100, reinterpret_cast<WPARAM>(p), 0);
        } else {
            std::cout << "Received frame: " << frame->width << "x" << frame->height
                      << " full=" << frame->isFullFrame << std::endl;
        }
    });
}

// 停止监视功能
void MonitorController::stopMonitoring() {
    if (!model_) return;
    model_->stopCapture();
    model_->stopRecording();
}

// ---- IMonitorController implementations ----
void MonitorController::OnStartCapture(int fps) {
    if (!model_) return;
    model_->startCapture(fps, [this](std::shared_ptr<const FrameData> frame) {
        if (!frame) return;
        if (viewHwnd_) {
            auto p = new std::shared_ptr<const FrameData>(frame);
            ::PostMessage(viewHwnd_, WM_APP + 0x100, reinterpret_cast<WPARAM>(p), 0);
        }
    });
}

void MonitorController::OnStopCapture() { if (model_) model_->stopCapture(); }

void MonitorController::OnStartRecording(const std::string &dir) { if (model_) model_->startRecording(dir); }

void MonitorController::OnStopRecording() { if (model_) model_->stopRecording(); }

void MonitorController::OnMouseInput(int x, int y, int button, bool down) {
    if (model_) model_->injectMouse(x, y, button, down);
}

void MonitorController::OnMouseMove(int x, int y) { if (model_) model_->injectMouse(x, y, 0, false); }

void MonitorController::OnKeyInput(int keycode, bool down) { if (model_) model_->injectKey(keycode, down); }

void MonitorController::OnMouseControlToggle(bool enabled) { /* controller-level state: no-op for now */ }

void MonitorController::OnKeyboardControlToggle(bool enabled) { /* no-op */ }

void MonitorController::OnSaveScreenshot(const std::string &savePath) { /* not implemented in model */ }

void MonitorController::OnSetZoom(float scale) { /* view handles zoom */ }

void MonitorController::OnFitToWindow() { /* view handles fit */ }

void MonitorController::OnActualSize() { /* view handles actual size */ }

void MonitorController::OnLockScreen() {
    if (io_) io_->sendLockCommand(true);
}

void MonitorController::OnUnlockScreen() {
    if (io_) io_->sendLockCommand(false);
}