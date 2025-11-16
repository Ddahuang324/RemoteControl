// Enities.h - 命令字定义 (服务端)
#pragma once

// 与客户端保持一致的命令字枚举（与远控/RemoteControl_server.cpp 中的数字映射一致）
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
