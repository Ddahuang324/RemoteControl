#pragma once

#include "../Protocol/Infra/SocketProtocol.h"
#include "Packet.hpp"
#include <optional>
#include <string>
#include <vector>


class Socket : public SocketBase {
public:
  static constexpr size_t kTempBufferSize = 1024 * 1024;      // 1 MB chunk per recv
  static constexpr size_t kMaxTotalBuffer = 20 * 1024 * 1024; // 20 MB total buffered data

  static Socket &getInstance() {
    static Socket instance;
    return instance;
  }

  // Disable copy and assignment
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  // 连接/关闭/发送/接收 (对外接口)
  bool connectToServer(const std::string &ip, unsigned short port);
  void CloseSocket();
  bool SendPacket(const Packet &packet);
  std::optional<Packet> RecvPacket(); // 低级接收接口（可与服务器对称）

  Socket();

  std::optional<Packet> TryRecvOnce();

public:
  // ---------- 低级对外接口（仅做连接/发送/一次接收/缓冲管理） ----------
  ~Socket();
  void ClearRecvBuffer();

private:
  std::vector<char> tmpBuffer_;
};
