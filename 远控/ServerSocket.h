#pragma once
#include "framework.h"
#include "pch.h"
#include <memory>
#include <string>
#include <vector>
#include <ws2tcpip.h>

#define BUFFER_SIZE (4 * 1024 * 1024)

class Cpacket {
public:
  Cpacket() : sHead(0), nLength(0), sCmd(0), sSum(0) {}
  Cpacket(WORD nCmd, const BYTE *pData, size_t nSize) {
    sHead = 0xFEFF;
    nLength = (DWORD)(nSize + 2 + 2);
    sCmd = nCmd;

    if (nSize > 0) {
      strData.resize(nSize);
      memcpy((void *)strData.c_str(), pData, nSize);
    } else {
      strData.clear();
    }
    sSum = 0;
    for (size_t j = 0; j < strData.size(); j++) {
      sSum += BYTE(strData[j]) & 0xFF;
    }
  }
  Cpacket(const Cpacket &pkt) {
    sHead = pkt.sHead;
    nLength = pkt.nLength;
    sCmd = pkt.sCmd;
    strData = pkt.strData;
    sSum = pkt.sSum;
  }
  Cpacket(const BYTE *pData, size_t &nSize) {
    size_t i = 0;

    for (; i < nSize; i++) {
      if (*(WORD *)((char *)pData + i) == 0xFEFF) {
        sHead = *(WORD *)((char *)pData + i);
        i += 2;
        break;
      }
    }

    if (i + 4 + 2 + 2 > nSize) {
      nSize = 0;
      return;
    }

    nLength = *(DWORD *)((char *)pData + i);
    i += 4;
    sCmd = *(WORD *)((char *)pData + i);
    i += 2;
    if (i + (nLength - 2 - 2) + 2 > nSize) {
      nSize = 0;
      return;
    }

    if (nLength > 4) {
      strData.resize(nLength - 2 - 2);
      memcpy((void *)strData.c_str(), (char *)pData + i, nLength - 2 - 2);
      i += nLength - 2 - 2;
    }
    sSum = *(WORD *)((char *)pData + i);
    i += 2;
    WORD sum = 0;
    for (size_t j = 0; j < strData.size(); j++) {
      sum += BYTE(strData[j]) & 0xFF;
    }
    if (sum == sSum) {
      nSize = i;
      return;
    }
    nSize = 0;
  }
  ~Cpacket() {}
  Cpacket &operator=(const Cpacket &pkt) {
    if (this != &pkt) {
      sHead = pkt.sHead;
      nLength = pkt.nLength;
      sCmd = pkt.sCmd;
      strData = pkt.strData;
      sSum = pkt.sSum;
    }
    return *this;
  }

  int Size() { return nLength + 2 + 4; }

  const char *Data() {
    strOut.resize(nLength + 6);
    BYTE *pData = (BYTE *)strOut.c_str();
    *(WORD *)pData = sHead;
    pData += 2;
    *(DWORD *)pData = nLength;
    pData += 4;
    *(WORD *)pData = sCmd;
    pData += 2;
    memcpy(pData, strData.c_str(), strData.size());
    pData += strData.size();
    *(WORD *)pData = sSum;

    return strOut.c_str();
  }

public:
  WORD sHead;
  DWORD nLength;
  WORD sCmd;
  std::string strData;
  WORD sSum;

  std::string strOut;
};

typedef struct MouseEvent {
  MouseEvent() : nAction(0), nButton(-1) {
    ptXY.x = 0;
    ptXY.y = 0;
  }
  WORD nAction;
  WORD nButton;
  POINT ptXY;
} MOUSEEVENT, *PMOUSEEVENT;

class CServerSocket {
private:
  CServerSocket(const CServerSocket &ss) {
    m_client = ss.m_client;
    m_serv = ss.m_serv;
  }
  CServerSocket &operator=(const CServerSocket &ss) = default;
  CServerSocket() {
    m_serv = INVALID_SOCKET;
    m_client = INVALID_SOCKET;

    if (InitSocketEnv() == false) {
      MessageBoxW(NULL, L"Initialization failed", L"Error", MB_OK);
      exit(0);
    }

    m_serv = socket(PF_INET, SOCK_STREAM, 0);
    if (m_serv == INVALID_SOCKET) {
      MessageBoxW(NULL, L"Socket creation failed", L"Error", MB_OK);
      WSACleanup();
      exit(0);
    }
    printf("[ServerSocket] created server socket: %llu\n",
           (unsigned long long)m_serv);
    fflush(stdout);
    if (InitSocketEnv() == false) {
      MessageBoxW(NULL, L"Initialization failed", L"Error", MB_OK);
      exit(0);
    }
    m_Buffer.resize(BUFFER_SIZE);
    m_nBufferIndex = 0;
  }
  ~CServerSocket() {
    closesocket(m_serv);
    WSACleanup();
  };

  bool InitSocketEnv() {
    WSADATA data;
    if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
      return false;
    }
    return true;
  };

  SOCKET m_serv;
  SOCKET m_client;
  Cpacket m_packet;
  std::vector<char> m_Buffer;
  size_t m_nBufferIndex;

public:
  static void CleanUp() { WSACleanup(); }
  static CServerSocket &GetInstance() {
    static CServerSocket instance;
    return instance;
  }

  bool initSocket() {
    if (m_serv == -1) {
      return false;
    }

    sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(12345);

    if (bind(m_serv, (SOCKADDR *)&serv_addr, sizeof(SOCKADDR)) == -1) {
      printf("[ServerSocket] bind failed on port %d\n",
             ntohs(serv_addr.sin_port));
      fflush(stdout);
      return false;
    }
    if (listen(m_serv, 1) == -1) {
      printf("[ServerSocket] listen failed\n");
      fflush(stdout);
      return false;
    }

    printf("[ServerSocket] listening on port %d\n", ntohs(serv_addr.sin_port));
    fflush(stdout);
    return true;
  }
  bool AcceptClient() {
    sockaddr_in client_addr;
    int client_addr_size = sizeof(SOCKADDR);
    m_client = accept(m_serv, (SOCKADDR *)&client_addr, &client_addr_size);
    if (m_client == INVALID_SOCKET)
      return false;

    char ip[INET_ADDRSTRLEN] = {0};
    if (inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip)) == nullptr) {
      strcpy_s(ip, sizeof(ip), "unknown");
    }
    unsigned short port = ntohs(client_addr.sin_port);
    printf("[ServerSocket] accepted client: %s:%u (sock=%llu)\n", ip,
           (unsigned)port, (unsigned long long)m_client);
    fflush(stdout);
    m_nBufferIndex = 0;
    return true;
  }

  int DealCommand() {
    if (m_client == -1)
      return false;

    // Use member buffer m_Buffer and m_nBufferIndex

    while (true) {
      // Try to parse packet from existing data
      if (m_nBufferIndex > 0) {
        size_t remaining = m_nBufferIndex;
        m_packet =
            Cpacket(reinterpret_cast<const BYTE *>(m_Buffer.data()), remaining);

        if (remaining > 0) {
          printf("[ServerSocket] parsed packet, cmd=%u\n",
                 (unsigned)m_packet.sCmd);
          fflush(stdout);

          size_t processed = remaining;
          if (processed < m_nBufferIndex) {
            memmove(m_Buffer.data(), m_Buffer.data() + processed,
                    m_nBufferIndex - processed);
            m_nBufferIndex -= processed;
          } else {
            m_nBufferIndex = 0;
          }
          return m_packet.sCmd;
        }
      }

      // Check for buffer overflow
      if (m_nBufferIndex >= BUFFER_SIZE - 1) {
        printf("[ServerSocket] buffer overflow, resetting buffer\n");
        fflush(stdout);
        m_nBufferIndex = 0;
        return -1;
      }

      int recvLen = recv(m_client, m_Buffer.data() + m_nBufferIndex,
                         (int)(BUFFER_SIZE - m_nBufferIndex), 0);
      if (recvLen <= 0) {
        printf(
            "[ServerSocket] recv returned %d (client disconnected or error)\n",
            recvLen);
        fflush(stdout);
        return -1;
      }

      m_nBufferIndex += recvLen;
      // Loop back to try parsing
    }
    return -1;
  }

  bool Send(const void *pData, size_t size) {
    if (m_client == -1)
      return false;
    int ret = (int)send(m_client, (const char *)pData, (int)size, 0);
    printf("[ServerSocket] send -> client sock=%llu, bytes=%zu, ret=%d\n",
           (unsigned long long)m_client, size, ret);
    fflush(stdout);
    return ret > 0;
  }
  bool Send(Cpacket &pkt) {
    if (m_client == -1)
      return false;
    int ret = (int)send(m_client, pkt.Data(), pkt.Size(), 0);
    printf(
        "[ServerSocket] send(pkt) -> client sock=%llu, pkt_size=%d, ret=%d\n",
        (unsigned long long)m_client, pkt.Size(), ret);
    fflush(stdout);
    return ret > 0;
  }

  bool GetFilePath(std::string &strPath) {
    // include delete-file command (11) as it also carries a file path
    if ((m_packet.sCmd == 2) || (m_packet.sCmd == 3) || (m_packet.sCmd == 4) ||
        (m_packet.sCmd == 11)) {
      strPath = m_packet.strData;
      return true;
    }
    return false;
  }

  bool GetMouseEvent(MOUSEEVENT &mouse) {
    if (m_packet.sCmd == 5) {
      memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEVENT));
      return true;
    }
    return false;
  }

  operator bool() const { return m_client != INVALID_SOCKET; }

  Cpacket &GetPacket() { return m_packet; }
  void CloseClient() {
    printf("[ServerSocket] closing client socket: %llu\n",
           (unsigned long long)m_client);
    fflush(stdout);
    closesocket(m_client);
  }
};
