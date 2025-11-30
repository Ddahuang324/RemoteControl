#include "IOModel.h"
#include "Packet.hpp"
#include "../../Enities.h"
#include <iostream>

IOModel::IOModel(std::shared_ptr<INetworkModel> net)
    : net_(std::move(net)) {}

IOModel::~IOModel() = default;

void IOModel::injectMouse(int x, int y, int button, bool down) {
    if (!net_)
        return;

    try {
        MouseEventData me;
        // Monitor/Server 协议约定：nAction: 2=down,3=up,4=move
        if (button == 0xFFFF) {
            me.nAction = 4; // move
        } else {
            me.nAction = down ? 2 : 3;
        }
        me.nButton = static_cast<WORD>(button & 0xFFFF);
        me.ptXY.x = x;
        me.ptXY.y = y;

        std::vector<BYTE> data = me.Serialize();
        Packet pkt;
        pkt.sCmd = static_cast<WORD>(CMD_MOUSE_EVENT);
        pkt.data = std::move(data);
        net_->sendPacket(pkt);
    } catch (const std::exception &e) {
        std::cerr << "IOModel::injectMouse exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "IOModel::injectMouse unknown exception" << std::endl;
    }
}

void injectKey(int keycode, bool down) {

    // 回退：目前没有定义的 key 注入格式，保留为 no-op
    (void)keycode; (void)down;
}
