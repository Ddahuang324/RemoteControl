#pragma once

#include <memory>
#include <string>
#include <atomic>

// 前向声明，避免强依赖头文件
class INetworkModel;
class IFileSystemModel;
class IMonitorModel;
class IIoModel;
class RemoteControlViewDlg;
class MonitorViewDlg;

namespace ControllerProtocol {

    struct MainControllerProtocol {
        // Models
        std::shared_ptr<INetworkModel> netModel;
        std::shared_ptr<IFileSystemModel> fsModel;
        std::shared_ptr<IMonitorModel> monitorModel;
        std::shared_ptr<IIoModel> ioModel;

        // Views (MFC 窗口句柄由 View 管理，Controller 持有原始指针)
        RemoteControlViewDlg* mainView = nullptr;
        MonitorViewDlg* monitorView = nullptr;

        // 内部状态
        std::string lastConnectedIP;
        uint16_t lastConnectedPort = 0;
        std::atomic_bool isConnecting{false};

        MainControllerProtocol() = default;
    };

    struct MonitorControllerProtocol {
        std::shared_ptr<IMonitorModel> monitorModel;
        std::shared_ptr<IIoModel> ioModel;
        void* viewHandler = nullptr;

        bool isInputEnabled = false;
        float currentZoom = 1.0f;
    };

}
