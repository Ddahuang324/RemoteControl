#include "pch.h"
#include "ConnectionController.h"
#include <iostream>
#include "../model/NetworkModel.h"

ConnectionController::ConnectionController(std::shared_ptr<INetworkModel> net)
    : net_(std::move(net)) {}

void ConnectionController::setNetworkModel(std::shared_ptr<INetworkModel> net) {
    net_ = std::move(net);
}

void ConnectionController::establishConnection(const std::string& ip, int port) {
    if (!net_) {
        // 自动创建一个默认的 NetworkModel 实例
        net_ = std::make_shared<NetworkModel>();
    }
    std::cout << "Connecting to server at " << ip << ":" << port << std::endl;
    bool ok = false;
    try {
        ok = net_->connectToServer(ip, static_cast<uint16_t>(port));
    } catch (...) {
        ok = false;
    }
    if (ok) {
        std::cout << "Connection established." << std::endl;
    } else {
        std::cout << "Failed to connect to server." << std::endl;
    }
}

void ConnectionController::disconnect() {
    if (!net_) {
        std::cout << "No network to disconnect." << std::endl;
        return;
    }
    std::cout << "Disconnecting from server." << std::endl;
    try {
        net_->disconnect();
    } catch (...) {
    }
}