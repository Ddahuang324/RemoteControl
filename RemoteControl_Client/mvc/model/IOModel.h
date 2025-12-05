#pragma once

#include "Interface.h"
#include <memory>

// IOModel: concrete IIoModel that uses INetworkModel to send
// serialized input events to the remote server.
class IOModel : public IIoModel {
public:
    explicit IOModel(std::shared_ptr<INetworkModel> net);
    ~IOModel() override;

    void injectMouse(int x, int y, int button, bool down) override;
    void injectKey(int keycode, bool down) override;
    void sendLockCommand(bool lock) override;

private:
    std::shared_ptr<INetworkModel> net_;
};
