// Enities.h - 命令字定义 (客户端)
#pragma once

// 与服务器实现保持一致的命令字枚举（使用 unsigned short）
enum CMD : unsigned short {
    CMD_NONE = 0,
    CMD_LIST_DRIVES = 1,
    CMD_LIST_DIR = 2,
    CMD_RUN_FILE = 3,
    CMD_DOWNLOAD_FILE = 4,
    CMD_MOUSE_EVENT = 5,
    CMD_SCREEN_CAPTURE = 6,
    CMD_LOCK_MACHINE = 7,
    CMD_UNLOCK_MACHINE = 8,
    CMD_DELETE_FILE = 11,
    CMD_EOF = 100,
    CMD_TEST_CONNECT = 2002,
    CMD_ERROR = 999
};

// 远程屏幕监控命令
// 10: 请求开始持续屏幕推送；13: 请求停止推送；6: 屏幕数据包（服务端发送的实际帧数据）
#define REQ_START_WATCH   10
#define REQ_STOP_WATCH    13
#define SCREEN_DATA_PACKET CMD::CMD_SCREEN_CAPTURE
