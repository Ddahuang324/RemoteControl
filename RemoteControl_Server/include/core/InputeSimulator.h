#pragma once

#include "../network/ServerSocket.h"
#include "../pch/pch.h"
#include "Enities.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

using BYTE = unsigned char;
using DWORD = unsigned long;

inline void SendInput(DWORD dwFlags) {
  INPUT input = {0};
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = dwFlags;

  ::SendInput(1, &input, sizeof(INPUT));
}

inline void HandleMouseEvent(CServerSocket &ClientSocket,
                             const Cpacket &packet) {
  try {
    MouseEventData mouse =
        MouseEventData::Deserialize(packet.data); // 逐字段反序列化

    if (!::SetCursorPos(mouse.ptXY.x, mouse.ptXY.y)) {
      std::string errMsg = "Failed to set cursor position.";
      ClientSocket.SendErrorPacket(errMsg);
      return;
    }

    DWORD dwFlags = 0;

    switch (mouse.nButton) {
    case 0:
      dwFlags = 1;
      break; // 左键
    case 1:
      dwFlags = 2;
      break; // 右键
    case 2:
      dwFlags = 4;
      break; // 中键
    case 3:
      dwFlags = 8;
      break; // 移动

    default:
      dwFlags = 8;
      break; // 默认为移动
    }

    switch (mouse.nAction) {
    case 0:
      dwFlags |= 0x10;
      break; // 单机
    case 1:
      dwFlags |= 0x20;
      break; // 双击
    case 2:
      dwFlags |= 0x40;
      break; // 按住
    case 3:
      dwFlags |= 0x80;
      break; // 释放
    case 4:
      break; // 移动

    default:
      break; // 默认移动
    }

    switch (dwFlags) {
    case 0x11: // 左键单击
      SendInput(MOUSEEVENTF_LEFTDOWN);
      SendInput(MOUSEEVENTF_LEFTUP);
      break;
    case 0x21: // 左键双击
      SendInput(MOUSEEVENTF_LEFTDOWN);
      SendInput(MOUSEEVENTF_LEFTUP);
      SendInput(MOUSEEVENTF_LEFTDOWN);
      SendInput(MOUSEEVENTF_LEFTUP);
      break;
    case 0x41: // 左键按住
      SendInput(MOUSEEVENTF_LEFTDOWN);
      break;
    case 0x81: // 左键释放
      SendInput(MOUSEEVENTF_LEFTUP);
      break;
    case 0x12: // 右键单击
      SendInput(MOUSEEVENTF_RIGHTDOWN);
      SendInput(MOUSEEVENTF_RIGHTUP);
      break;
    case 0x22: // 右键双击
      SendInput(MOUSEEVENTF_RIGHTDOWN);
      SendInput(MOUSEEVENTF_RIGHTUP);
      SendInput(MOUSEEVENTF_RIGHTDOWN);
      SendInput(MOUSEEVENTF_RIGHTUP);
      break;
    case 0x42: // 右键按住
      SendInput(MOUSEEVENTF_RIGHTDOWN);
      break;
    case 0x82: // 右键释放
      SendInput(MOUSEEVENTF_RIGHTUP);
      break;

    case 0x14: // 中键单击
      SendInput(MOUSEEVENTF_MIDDLEDOWN);
      SendInput(MOUSEEVENTF_MIDDLEUP);
      break;
    case 0x24: // 中键双击
      SendInput(MOUSEEVENTF_MIDDLEDOWN);
      SendInput(MOUSEEVENTF_MIDDLEUP);
      SendInput(MOUSEEVENTF_MIDDLEDOWN);
      SendInput(MOUSEEVENTF_MIDDLEUP);
      break;
    case 0x44: // 中键按住
      SendInput(MOUSEEVENTF_MIDDLEDOWN);
      break;
    case 0x84: // 中键释放
      SendInput(MOUSEEVENTF_MIDDLEUP);
      break;

    case 0x08: // 移动
      break;

    default:
      std::cout << "HandleMouseEvent: Invalid mouse event." << std::hex
                << dwFlags << std::endl;
      break;
    }
    Cpacket ackPacket(CMD::CMD_MOUSE_EVENT, mouse.Serialize());
    ClientSocket.SendPacket(ackPacket, SendPriority::HIGH); // 鼠标响应高优先级
  } catch (const std::exception &e) {
    std::cerr << "HandleMouseEvent exception: " << e.what() << std::endl;
    ClientSocket.SendErrorPacket(e.what());
  } catch (...) {
    std::cerr << "HandleMouseEvent unknown exception." << std::endl;
    ClientSocket.SendErrorPacket("Unknown error occurred in HandleMouseEvent.");
  }
}