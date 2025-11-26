#include "pch.h"
#include "MouseHandler.h"
#include "..\ServerSocket.h"
#include "..\RemoteControl_Server\include\core\Enities.h"

using namespace std;

namespace handlers {

int HandleMouseEvent(CServerSocket &socket) {
  MOUSEEVENT mouse;
  if (socket.GetMouseEvent(mouse)) {
    OutputDebugString(_T("获取鼠标事件成功"));

    DWORD nFlags = 0; // 鼠标标志位， 1 = 左键 2 = 右键 4 = 中键

    switch (mouse.nButton) {
    case 0:
      nFlags = 1;
      break;
    case 1:
      nFlags = 2;
      break;
    case 2:
      nFlags = 4;
      break;
    case 3:
      nFlags = 8;
      break;
    default:
      break;
    }

    if (nFlags != 8)
      SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);

    switch (mouse.nAction) {
    case 0:
      nFlags |= 0x10;
      break;
    case 1:
      nFlags |= 0x20;
      break;
    case 2:
      nFlags |= 0x40;
      break;
    case 3:
      nFlags |= 0x80;
      break;
    default:
      break;
    }

    switch (nFlags) {
    case 0x11:
      mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x21:
      mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x41:
      mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x81:
      mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x12:
      mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x22:
      mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x42:
      mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x82:
      mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x14:
      mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x24:
      mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
      mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x44:
      mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x84:
      mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
      break;
    case 0x08:
      mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
      break;
    default:
      break;
    }

    Cpacket pack(5, (BYTE *)&mouse, sizeof(mouse));
    socket.Send(pack); // 发送回执
    return 0;
  } else {
    OutputDebugString(_T("获取鼠标事件失败"));
    return -1;
  }
}

} // namespace handlers
