#pragma once
#include "pch.h"
#include "framework.h"

class CServerSocket
{ 
public:
    CServerSocket() {
        if (InitSocketEnv() == false) {
            // Initialization failed
            MessageBoxW(NULL, L"Initialization failed", L"Error", MB_OK);
            exit(0);
        }
    }
    ~CServerSocket() {
        WSACleanup();
    };
    bool InitSocketEnv() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
            return false;
        }
        return true;
    };

};

extern CServerSocket server;

