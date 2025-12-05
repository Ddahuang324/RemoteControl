#pragma once
#include "Enities.h"
#include "ServerSocket.h"
#include <unordered_map>
#include <functional>

class CommandDispatcher {
public:
    using Handler = std::function<void(const Cpacket&, CServerSocket&)>;
    CommandDispatcher();
    void Dispatch(const Cpacket& packet, CServerSocket& serverSocket) const;

private:
    std::unordered_map<int, Handler> handlers_;
};
