#include "pch.h"
#include "Socket.hpp"
#include <windows.h>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <cstring>
#include <fstream>
#include <chrono>
#include <utility>

namespace {
constexpr size_t kRecvChunkSize = Socket::kTempBufferSize;
constexpr size_t kMaxAccumulatedBuffer = Socket::kMaxTotalBuffer; // 总缓冲上限
}

static void SocketLog(const char *msg) {
  try {
    std::ofstream ofs("socket_debug.log", std::ios::app);
    if (ofs)
      ofs << msg << std::endl;
  } catch (...) {
  }
}

WSAInitializer::WSAInitializer() {
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    std::cerr << "Error: WSAStartup failed" << std::endl;
    throw std::runtime_error("WSAStartup failed");
  }
}

WSAInitializer::~WSAInitializer() { WSACleanup(); }

SOCKET CreateTcpSocket() { return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); }

bool SetSocketOptions(SOCKET s) {
  if (s == INVALID_SOCKET)
    return false;
  int flag = 1;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&flag),
             sizeof(flag));
  int rcv = 256 * 1024;
  setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&rcv),
             sizeof(rcv));
  return true;
}

bool ConnectSocket(SOCKET s, const std::string &ip, unsigned short port) {
  if (s == INVALID_SOCKET)
    return false;
  sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) != 1) {
    std::ostringstream ss;
    ss << "inet_pton failed for IP: " << ip;
    SocketLog(ss.str().c_str());
    return false;
  }
  if (connect(s, reinterpret_cast<sockaddr *>(&serverAddr),
              sizeof(serverAddr)) == SOCKET_ERROR) {
    std::ostringstream ss;
    ss << "connect() failed, WSAErr=" << WSAGetLastError();
    SocketLog(ss.str().c_str());
    return false;
  }
  return true;
}

bool SendAll(SOCKET s, const std::vector<BYTE> &buf) {
  if (s == INVALID_SOCKET)
    return false;
  const char *data = reinterpret_cast<const char *>(buf.data());
  int total = 0;
  int toSend = static_cast<int>(buf.size());
  while (total < toSend) {
    int ret = send(s, data + total, toSend - total, 0);
    if (ret == SOCKET_ERROR)
      return false;
    total += ret;
  }
  return true;
}

void CloseSocketGraceful(SOCKET &s) {
  if (s == INVALID_SOCKET)
    return;
  int rc = shutdown(s, SD_BOTH);
  {
    std::ostringstream ss;
    ss << "shutdown returned=" << rc << ", WSAErr=" << WSAGetLastError();
    SocketLog(ss.str().c_str());
  }
  int cl = closesocket(s);
  {
    std::ostringstream ss;
    ss << "closesocket returned=" << cl << ", WSAErr=" << WSAGetLastError();
    SocketLog(ss.str().c_str());
  }
  s = INVALID_SOCKET;
}

// -----------------------------
// Socket implementation (ported from network/ClientSocket.cpp)
// -----------------------------


static void EnsureReserve(std::vector<BYTE> &v, size_t minCap) {
  if (v.capacity() < minCap)
    v.reserve(minCap);
}

Socket::Socket() : tmpBuffer_(kTempBufferSize) {
  // wsaInit RAII will initialize Winsock
  EnsureReserve(recvBuffer, kTempBufferSize * 4);
}

Socket::~Socket() { CloseSocket(); }

bool Socket::connectToServer(const std::string &ip, unsigned short port) {
  // If connection exists, close first
  CloseSocket();

  servSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (servSocket == INVALID_SOCKET) {
    std::cerr << "Error: " << "Socket error" << std::endl;
    SocketLog("[Socket] socket() failed");
    return false;
  }

  // Set socket options
  int flag = 1;
  setsockopt(servSocket, IPPROTO_TCP, TCP_NODELAY,
             reinterpret_cast<const char *>(&flag), sizeof(flag));
  int rcv = 256 * 1024;
  setsockopt(servSocket, SOL_SOCKET, SO_RCVBUF,
             reinterpret_cast<const char *>(&rcv), sizeof(rcv));

  sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) != 1) {
    std::cerr << "Error: Invalid IP address" << std::endl;
    CloseSocket();
    SocketLog("[Socket] inet_pton failed for IP: ");
    SocketLog(ip.c_str());
    return false;
  }

  if (connect(servSocket, reinterpret_cast<sockaddr *>(&serverAddr),
              sizeof(serverAddr)) == SOCKET_ERROR) {
    std::cerr << "Error: Connect failed: " << WSAGetLastError() << std::endl;
    {
      std::ostringstream ss;
      ss << "[Socket] connect() failed, WSAErr=" << WSAGetLastError();
      SocketLog(ss.str().c_str());
    }
    CloseSocket();
    return false;
  }

  // Prepare buffers; 上层负责启动业务线程
  {
    std::lock_guard<std::mutex> lk(recvBufferMutex);
    recvBuffer.clear();
  }
  {
    std::ostringstream ss;
    ss << "[Socket] connected to " << ip << ":" << port;
    SocketLog(ss.str().c_str());
  }
  return true;
}

void Socket::CloseSocket() {
  SocketLog("[Socket] CloseSocket called");
  if (servSocket != INVALID_SOCKET) {
    int rc = shutdown(servSocket, SD_BOTH);
    {
      std::ostringstream ss;
      ss << "[Socket] shutdown returned=" << rc
         << ", WSAErr=" << WSAGetLastError();
      SocketLog(ss.str().c_str());
    }
    int cl = closesocket(servSocket);
    {
      std::ostringstream ss;
      ss << "[Socket] closesocket returned=" << cl
         << ", WSAErr=" << WSAGetLastError();
      SocketLog(ss.str().c_str());
    }
    servSocket = INVALID_SOCKET;
  }
}

bool Socket::SendPacket(const Packet &packet) {
  if (servSocket == INVALID_SOCKET)
    return false;
  std::vector<BYTE> buf = packet.SerializePacket();
  return SendAll(servSocket, buf);
}

std::optional<Packet> Socket::RecvPacket() { return TryRecvOnce(); }

std::optional<Packet> Socket::TryRecvOnce() {
  if (servSocket == INVALID_SOCKET)
    return std::nullopt;


  // 首先尝试从已有缓冲区解析包（可能上次recv已收到完整包但未解析）
  {
    std::unique_lock<std::mutex> rb_lk(recvBufferMutex);
    if (!recvBuffer.empty()) {
      size_t consumed = 0;
      auto opt = Packet::DeserializePacket(recvBuffer, consumed);
      if (opt) {
        Packet pkt = std::move(*opt);
        if (consumed > 0) {
          if (consumed <= recvBuffer.size())
            recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + consumed);
          else
            recvBuffer.clear();
        }
        return pkt;
      }
    }
  }

  // 缓冲区中没有完整包，尝试接收新数据
  // Check if socket has data (non-blocking)
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(servSocket, &readfds);
  timeval tv{0, 0};
  int sel = select(0, &readfds, nullptr, nullptr, &tv);
  if (sel <= 0)
    return std::nullopt;

  int ret = recv(servSocket, tmpBuffer_.data(), static_cast<int>(tmpBuffer_.size()), 0);
  if (ret > 0) {
    {
      std::ostringstream ss;
      ss << "[Socket] recv returned bytes=" << ret;
      SocketLog(ss.str().c_str());
    }
    {
      std::lock_guard<std::mutex> rb_lk(recvBufferMutex);
      recvBuffer.insert(recvBuffer.end(), tmpBuffer_.data(), tmpBuffer_.data() + ret);
      if (recvBuffer.size() > kMaxAccumulatedBuffer) {
        SocketLog("[Socket] recv buffer exceeded MAX_TOTAL_BUFFER, clearing");
        recvBuffer.clear();
      }
    }

    // try parse one packet
    std::unique_lock<std::mutex> rb_lk(recvBufferMutex);
    size_t consumed = 0;
    auto opt = Packet::DeserializePacket(recvBuffer, consumed);
    if (!opt) {
      if (consumed > 0) {
        std::ostringstream ss;
        ss << "[Socket] DeserializePacket incomplete/invalid, consumed="
           << consumed;
        SocketLog(ss.str().c_str());
        if (consumed <= recvBuffer.size())
          recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + consumed);
        else
          recvBuffer.clear();
      } else {
        SocketLog("[Socket] DeserializePacket: need more data");
      }
      return std::nullopt;
    }

    Packet pkt = std::move(*opt);
    if (consumed > 0) {
      if (consumed <= recvBuffer.size())
        recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + consumed);
      else
        recvBuffer.clear();
    }
    return pkt;
  } else if (ret == 0) {
    SocketLog("[Socket] recv returned 0 (peer closed)");
    CloseSocketGraceful(servSocket);
    return std::nullopt;
  } else {
    int err = WSAGetLastError();
    std::ostringstream ss;
    ss << "[Socket] recv error ret=" << ret << " WSAErr=" << err;
    SocketLog(ss.str().c_str());
    CloseSocketGraceful(servSocket);
    return std::nullopt;
  }
}

// legacy packet-queue related APIs moved to NetworkModel (no-ops or removed in
// low-level socket)
void Socket::ClearRecvBuffer() {
  std::lock_guard<std::mutex> rb_lk(recvBufferMutex);
  recvBuffer.clear();
}
// Note: ClearPacketsByCmd / ClearAllPackets are intentionally omitted from
// low-level socket.
