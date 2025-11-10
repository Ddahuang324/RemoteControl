#pragma once
#include "Enities.h"
#include "../network/ServerSocket.h"
#include <iostream>
#include <thread>
#include <atomic>
#include "Resource.h"
#include "../ui/LockDialog.h"


void LockMachine(CServerSocket& serverSocket, const Cpacket& packet);
void UnlockMachine(CServerSocket& serverSocket, const Cpacket& packet);
void ResetLockState(); // For testing purposes
bool IsMachineLocked();
DWORD GetLockThreadId();
bool IsLockDialogCreated();