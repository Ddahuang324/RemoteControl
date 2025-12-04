#pragma once

#include "../../include/Infra/Packet.hpp"
#include "../../include/Infra/Socket.hpp"
#include "Interface.h"
#include "../../include/Protocol/Infra/PacketProtocol.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>


// Concrete implementation of INetworkModel that composes Socket
class NetworkModel : public INetworkModel {
public:
  NetworkModel();
  ~NetworkModel() override;

  // INetworkModel
  bool connectToServer(const std::string &ip, uint16_t port) override;
  void disconnect() override;

  bool sendPacket(const Packet &pkt) override;
  std::optional<Packet> recvPacket() override;

  std::optional<Packet> getNextPacketBlocking(int timeoutMs = 15000) override;
  std::optional<Packet> getLatestPacket() override;

  void clearRecvBuffer() override;
  void clearPacketsByCmd(WORD cmd) override;
  void clearAllPackets() override;

  void setOnPacketReceived(PacketCb cb) override;
  void setOnStatusChanged(StatusCb cb) override;

private:
  void RecvThreadLoop();

  Packet ToPublic(const Packet &p);
  Packet ToInternal(const Packet &p);
};
