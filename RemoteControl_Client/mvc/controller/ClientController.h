#pragma once

#include <string>
#include <memory>
#include "../model/Interface.h"
#include "ConnectionController.h"
#include "FileController.h"
#include "MonitorController.h"

class ClientController {
public:
    explicit ClientController(std::shared_ptr<INetworkModel> net = nullptr);
    void connectToServer(const std::string& ip, int port);
    void listDirectory(const std::string& path);
    void downloadFile(const std::string& filePath);
    void deleteFile(const std::string& filePath);
    void startMonitoring();
    void stopMonitoring();

private:
    std::shared_ptr<INetworkModel> net_;
    std::shared_ptr<IFileSystemModel> fsModel_;
    std::shared_ptr<IMonitorModel> monitorModel_;

    std::shared_ptr<ConnectionController> connCtrl_;
    std::shared_ptr<FileController> fileCtrl_;
    std::shared_ptr<MonitorController> monCtrl_;
};