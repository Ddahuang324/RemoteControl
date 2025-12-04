#include "pch.h"
#include "ClientController.h"
#include "../model/NetworkModel.h"
#include "../model/FileSystemModel.h"
#include "../model/MoniterModel.h"
#include "../model/IOModel.h"
#include <iostream>

ClientController::ClientController(std::shared_ptr<INetworkModel> net)
    : net_(std::move(net)) {
    if (!net_) net_ = std::make_shared<NetworkModel>();

    fsModel_ = std::make_shared<FileSystemModel>(net_);
    monitorModel_ = std::make_shared<MonitorModel>();

    connCtrl_ = std::make_shared<ConnectionController>(net_);
    fileCtrl_ = std::make_shared<FileController>(fsModel_);
    monCtrl_ = std::make_shared<MonitorController>(monitorModel_);

    // inject io model into monitorModel (uses network)
    auto io = std::make_shared<IOModel>(net_);
    monitorModel_->setIoModel(io);
    monCtrl_->setIoModel(io);
}

void ClientController::connectToServer(const std::string& ip, int port) {
    connCtrl_->establishConnection(ip, port);
}

void ClientController::listDirectory(const std::string& path) {
    if (!fsModel_) return;
    fsModel_->listDirectory(path, [](const std::vector<FileSystemProtocol::FileEntry> &entries, bool hasMore) {
        std::cout << "Directory batch (count=" << entries.size() << ") hasMore=" << hasMore << std::endl;
        for (const auto &e : entries) {
            std::cout << (e.isDirectory ? "[D] " : "[F] ") << e.name << " path=" << e.fullPath << std::endl;
        }
    });
}

void ClientController::downloadFile(const std::string& filePath) {
    if (!fileCtrl_) return;
    fileCtrl_->downloadFile(filePath);
}

void ClientController::deleteFile(const std::string& filePath) {
    if (!fileCtrl_) return;
    fileCtrl_->deleteFile(filePath);
}

void ClientController::startMonitoring() {
    if (!monCtrl_) return;
    monCtrl_->startMonitoring();
}

void ClientController::stopMonitoring() {
    if (!monCtrl_) return;
    monCtrl_->stopMonitoring();
}