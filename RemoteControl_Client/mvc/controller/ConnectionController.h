// ConnectionController.h: 头文件
//

#pragma once
#include <string>
#include <memory>
#include "../model/Interface.h"

class ConnectionController {
public:
    explicit ConnectionController(std::shared_ptr<INetworkModel> net = nullptr);
    void establishConnection(const std::string& ip, int port);
    void disconnect();
    void setNetworkModel(std::shared_ptr<INetworkModel> net);

private:
    std::shared_ptr<INetworkModel> net_;
};