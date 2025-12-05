#pragma once

#include <vector>
#include <map>
#include <string>
#include <memory>
#include <mutex>
#include <afxwin.h>

namespace ViewProtocol {

    struct LayoutResource {
        struct Anchor {
            int nID;
            bool left = false;
            bool top = false;
            bool right = false;
            bool bottom = false;
        };
        std::vector<Anchor> rules;
        std::map<int, CRect> originalRects;
        CRect originalDialogRect;
    };

    struct MainViewState {
        bool isConnected = false;
        bool isMonitoring = false;
        bool isFileListCleared = false;

        CString currentPath;
        CString selectedFile;
        CString lastSelectedPath;
    };

    struct SystemResource {
        HICON hIcon = nullptr;
    };

    struct MainViewProtocol {
        MainViewProtocol() {
            layout = std::make_unique<LayoutResource>();
            state = std::make_unique<MainViewState>();
            sysRes = std::make_unique<SystemResource>();
        }

        std::unique_ptr<LayoutResource> layout;
        std::unique_ptr<MainViewState> state;
        std::unique_ptr<SystemResource> sysRes;
    };

}
