#pragma once

#include <vector>
#include <memory>
#include <stdexcept>
#include <comutil.h>
#include <iostream>
#include <sstream>

#include "pch.h"
#include "Enities.h"
#include "../network/ServerSocket.h"
#include "diffAlgorithm.h"

using namespace Gdiplus;

// RAII: window DC acquired by GetDC/ReleaseDC
class ScopedWindowDC {
public:
    explicit ScopedWindowDC(HWND hwnd) : m_hwnd(hwnd), m_hdc(::GetDC(hwnd)) {}
    ~ScopedWindowDC() { if (m_hdc) { ::ReleaseDC(m_hwnd, m_hdc); } }
    HDC get() const { return m_hdc; }
    // non-copyable
    ScopedWindowDC(const ScopedWindowDC&) = delete;
    ScopedWindowDC& operator=(const ScopedWindowDC&) = delete;
private:
    HWND m_hwnd{};
    HDC m_hdc{};
};

// 辅助函数：捕获屏幕图像并返回图像和像素数据
inline std::tuple<std::shared_ptr<CImage>, std::vector<BYTE>, int, int, int> CaptureScreenImage() {
    // 使用 DISPLAY 设备环境，优先尝试 CreateDC("DISPLAY")，但在失败或 BitBlt 效果不佳时回退到 GetDC(NULL)
    ScopedHDC hDisplayDC(CreateDC(L"DISPLAY", nullptr, nullptr, nullptr));
    if (!hDisplayDC) {
        std::cout << "CaptureScreenImage: CreateDC(\"DISPLAY\") failed, falling back to GetDC(NULL)" << std::endl;
    }

    // 使用虚拟屏幕指标，覆盖所有显示器，并考虑原点偏移（可能为负）
    const int originX = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int originY = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int nWidth  = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int nHeight = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // 颜色深度从显示 DC 获取（仅用于日志）
    const int nBitperPixel = ::GetDeviceCaps(hDisplayDC.get() ? hDisplayDC.get() : ::GetDC(NULL), BITSPIXEL);

    if (nWidth <= 0 || nHeight <= 0) {
        throw std::runtime_error("Invalid virtual screen dimensions.");
    }

    auto ScreenImage = std::make_shared<CImage>();
    // 为与差异算法保持一致，强制采集为 32bpp
    const int createdBpp = 32;
    HRESULT hrCreate = ScreenImage->Create(nWidth, nHeight, createdBpp);
    if (FAILED(hrCreate)) {
        DWORD lastError = GetLastError();
        std::ostringstream oss;
        oss << "Failed to create CImage." 
            << " w=" << nWidth << " h=" << nHeight
            << " screenBpp=" << nBitperPixel
            << " hr=0x" << std::hex << hrCreate
            << " lastError=" << std::dec << lastError;
        throw std::runtime_error(oss.str());
    }

    HDC hImageDC = ScreenImage->GetDC();
    if (!hImageDC) {
        throw std::runtime_error("Failed to get image DC.");
    }

    // 从虚拟屏幕原点开始拷贝整块区域，确保全覆盖
    bool bltOk = false;
    if (hDisplayDC) {
        bltOk = (::BitBlt(hImageDC, 0, 0, nWidth, nHeight, hDisplayDC.get(), originX, originY, SRCCOPY) != FALSE);
        std::cout << "CaptureScreenImage: tried BitBlt from DISPLAY DC -> " << (bltOk ? "OK" : "FAILED") << std::endl;
    }
    if (!bltOk) {
        // fallback to GetDC(NULL) which usually represents the desktop
        HDC hScreen = ::GetDC(NULL);
        if (hScreen) {
            bool blt2 = (::BitBlt(hImageDC, 0, 0, nWidth, nHeight, hScreen, originX, originY, SRCCOPY) != FALSE);
            std::cout << "CaptureScreenImage: tried BitBlt from GetDC(NULL) -> " << (blt2 ? "OK" : "FAILED") << std::endl;
            ::ReleaseDC(NULL, hScreen);
            if (!blt2) {
                ScreenImage->ReleaseDC();
                throw std::runtime_error("BitBlt failed in CaptureScreenImage with both DISPLAY and GetDC(NULL).");
            }
        } else {
            ScreenImage->ReleaseDC();
            throw std::runtime_error("CaptureScreenImage: GetDC(NULL) returned NULL during fallback.");
        }
    }
    ScreenImage->ReleaseDC();

    // 动态计算 stride（考虑字节对齐），使用实际创建的位深（固定 32bpp）
    const int stride = ((nWidth * createdBpp + 31) / 32) * 4;
    BYTE* currentPixels = reinterpret_cast<BYTE*>(ScreenImage->GetBits());
    if (!currentPixels) {
        throw std::runtime_error("CaptureScreenImage: GetBits returned nullptr.");
    }
    std::vector<BYTE> currentFramePixels(currentPixels, currentPixels + static_cast<size_t>(nHeight) * stride);

    // 返回实际使用的位深，以与上层处理保持一致
    return {ScreenImage, currentFramePixels, nWidth, nHeight, createdBpp};
}

inline void CaptureScreen(CServerSocket& ClientSocket, const Cpacket& packet) {
    static std::vector<BYTE> previousFramePixels; // 存储上一帧的像素数据
    static int prevWidth = 0, prevHeight = 0;

    try {
        auto [ScreenImagePtr, currentFramePixels, nWidth, nHeight, nBitperPixel] = CaptureScreenImage();
        auto& ScreenImage = *ScreenImagePtr;

        // 使用算法工厂生成屏幕数据（检测差异并创建数据包）
        std::vector<BYTE> screenData = GenerateScreenData(ScreenImage, previousFramePixels, currentFramePixels, nWidth, nHeight, nBitperPixel);

        std::cout << "Screen data size: " << screenData.size() << " bytes" << std::endl;

        Cpacket screenPacket(CMD::CMD_SCREEN_CAPTURE, screenData);
        
        ClientSocket.SendPacket(screenPacket);

        // 更新上一帧
        previousFramePixels = std::move(currentFramePixels);
        prevWidth = nWidth;
        prevHeight = nHeight;
    }
    catch (const std::exception& ex) {
        std::string errMsg = "Exception caught: " + std::string(ex.what());
        ClientSocket.SendErrorPacket(errMsg);  
        return;
    }
    catch (...) {
        std::string errMsg = "Unknown exception caught.";
        ClientSocket.SendErrorPacket(errMsg);  
        return;
    }
}