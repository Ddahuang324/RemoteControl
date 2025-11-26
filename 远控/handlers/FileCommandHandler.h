#pragma once

#include <cstddef>
#include <string>

class CServerSocket;

namespace handlers {
// free functions for file/drive related commands
// Implementations operate directly on the provided socket and no longer
// require a FileManager helper object. This removes duplicate code paths.
int HandleMakeDriverInfo(CServerSocket &socket);
int HandleMakeDirectoryInfo(CServerSocket &socket);
int HandleRunFile(CServerSocket &socket);
int HandleDeleteFile(CServerSocket &socket);
int HandleDownloadFile(CServerSocket &socket);
} // namespace handlers
