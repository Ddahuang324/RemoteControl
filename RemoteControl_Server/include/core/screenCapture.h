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

// Helper: SEH-protected memcpy implemented in a function without C++ automatic objects
#ifdef _MSC_VER
static __declspec(noinline) bool SEH_Memcpy_NoObj(void* dst, const void* src, size_t size, unsigned int* pExceptionCode) {
    // This function must not contain C++ objects with non-trivial destructors
    __try {
        memcpy(dst, src, size);
        if (pExceptionCode) *pExceptionCode = 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (pExceptionCode) *pExceptionCode = ::GetExceptionCode();
        return false;
    }
}
#else
static bool SEH_Memcpy_NoObj(void* dst, const void* src, size_t size, unsigned int* pExceptionCode) {
    try {
        memcpy(dst, src, size);
        if (pExceptionCode) *pExceptionCode = 0;
        return true;
    } catch (...) {
        if (pExceptionCode) *pExceptionCode = 1;
        return false;
    }
}
#endif

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
inline std::tuple<std::shared_ptr<CImage>, BYTE*, int, int, int> CaptureScreenImage() {
    // 使用 DISPLAY 设备环境，优先尝试 CreateDC("DISPLAY")，但在失败或 BitBlt 效果不佳时回退到 GetDC(NULL)
    ScopedHDC hDisplayDC(CreateDC(L"DISPLAY", nullptr, nullptr, nullptr));
    if (!hDisplayDC) {
        std::cout << "CaptureScreenImage: CreateDC(\"DISPLAY\") failed, falling back to GetDC(NULL)" << std::endl;
    } else {
        std::cout << "CaptureScreenImage: CreateDC(\"DISPLAY\") succeeded, HDC=" << hDisplayDC.get() << std::endl;
    }

    // 尝试将进程设置为 DPI 感知，以确保后续的坐标/尺寸为物理像素
    bool dpiAwareSet = false;
    HMODULE hUser32 = ::GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI* SetProcessDPIAware_t)();
        auto pSetProcessDPIAware = reinterpret_cast<SetProcessDPIAware_t>(::GetProcAddress(hUser32, "SetProcessDPIAware"));
        if (pSetProcessDPIAware) {
            dpiAwareSet = (pSetProcessDPIAware() != FALSE);
        }
    }

    // 优先捕获主显示器（全屏），避免使用虚拟屏幕导致的坐标偏移
    int originX = 0;
    int originY = 0;
    int nWidth = 0;
    int nHeight = 0;
    HMONITOR hPrimary = ::MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (::GetMonitorInfo(hPrimary, &mi)) {
        originX = mi.rcMonitor.left;
        originY = mi.rcMonitor.top;
        nWidth = mi.rcMonitor.right - mi.rcMonitor.left;
        nHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
        std::cout << "CaptureScreenImage: using primary monitor rc=(" << mi.rcMonitor.left << "," << mi.rcMonitor.top << ")-(" << mi.rcMonitor.right << "," << mi.rcMonitor.bottom << ") dpiAwareSet=" << (dpiAwareSet ? "Y" : "N") << std::endl;
    }
    else {
        // 回退到虚拟屏幕（原行为）
        originX = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
        originY = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
        nWidth  = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
        nHeight = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
        std::cout << "CaptureScreenImage: GetMonitorInfo failed, falling back to virtual screen origin=(" << originX << "," << originY << ") size=(" << nWidth << "," << nHeight << ")" << std::endl;
    }

    // 颜色深度从显示 DC 获取（仅用于日志）
    // 注意：避免在表达式中直接调用 GetDC(NULL) 导致无法释放的 HDC
    HDC tempDCForCaps = hDisplayDC.get();
    HDC hScreenDCForCaps = nullptr;
    if (!tempDCForCaps) {
        hScreenDCForCaps = ::GetDC(NULL);
        tempDCForCaps = hScreenDCForCaps;
    }
    const int nBitperPixel = ::GetDeviceCaps(tempDCForCaps, BITSPIXEL);
    std::cout << "CaptureScreenImage: DeviceCaps BITSPIXEL=" << nBitperPixel << " tempDCForCaps=" << tempDCForCaps << std::endl;
    if (hScreenDCForCaps) {
        ::ReleaseDC(NULL, hScreenDCForCaps);
    }

    if (nWidth <= 0 || nHeight <= 0) {
        throw std::runtime_error("Invalid virtual screen dimensions.");
    }

    auto ScreenImage = std::make_shared<CImage>();
    // 为与差异算法保持一致，强制采集为 32bpp
    const int createdBpp = 32;
    HRESULT hrCreate = ScreenImage->Create(nWidth, nHeight, createdBpp);
    std::cout << "CaptureScreenImage: ScreenImage->Create returned hr=0x" << std::hex << hrCreate << std::dec << std::endl;
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
    std::cout << "CaptureScreenImage: ScreenImage->GetDC() returned HDC=" << hImageDC << std::endl;
    if (!hImageDC) {
        throw std::runtime_error("Failed to get image DC.");
    }

    // 从虚拟屏幕原点开始拷贝整块区域，确保全覆盖
    bool bltOk = false;
    if (hDisplayDC) {
        bltOk = (::BitBlt(hImageDC, 0, 0, nWidth, nHeight, hDisplayDC.get(), originX, originY, SRCCOPY) != FALSE);
        std::cout << "CaptureScreenImage: tried BitBlt from DISPLAY DC -> " << (bltOk ? "OK" : "FAILED") << " (srcHDC=" << hDisplayDC.get() << ", dstHDC=" << hImageDC << ")" << std::endl;
    }
    if (!bltOk) {
        // fallback to GetDC(NULL) which usually represents the desktop
        HDC hScreen = ::GetDC(NULL);
        if (hScreen) {
            bool blt2 = (::BitBlt(hImageDC, 0, 0, nWidth, nHeight, hScreen, originX, originY, SRCCOPY) != FALSE);
            std::cout << "CaptureScreenImage: tried BitBlt from GetDC(NULL) -> " << (blt2 ? "OK" : "FAILED") << " (srcHDC=" << hScreen << ", dstHDC=" << hImageDC << ")" << std::endl;
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
    std::cout << "CaptureScreenImage: calling ScreenImage->ReleaseDC() for image HDC=" << hImageDC << std::endl;
    ScreenImage->ReleaseDC();
    std::cout << "CaptureScreenImage: ReleaseDC done." << std::endl;

    // 动态计算 stride（考虑字节对齐），使用实际创建的位深（固定 32bpp）
    const int stride = ((nWidth * createdBpp + 31) / 32) * 4;
    BYTE* currentPixels = reinterpret_cast<BYTE*>(ScreenImage->GetBits());
    if (!currentPixels) {
        throw std::runtime_error("CaptureScreenImage: GetBits returned nullptr.");
    }
    size_t expectedSize = static_cast<size_t>(nHeight) * stride;
    std::cout << "CaptureScreenImage: stride=" << stride << " expectedSize=" << expectedSize << " bitsPtr=" << static_cast<void*>(currentPixels) << std::endl;

    // 不再在此处分配并返回像素 vector，改为直接返回指向 CImage 内部像素的指针。
    // 上层调用者可使用 FrameBufferPool 获取缓冲并将数据复制到可复用缓冲中，以避免每帧 heap 分配。
    if (!currentPixels) {
        throw std::runtime_error("CaptureScreenImage: GetBits returned nullptr.");
    }
    return {ScreenImage, currentPixels, nWidth, nHeight, createdBpp};
}

// CaptureService: 持有上一帧状态以避免在函数中使用静态变量
class CaptureService {
public:
    static CaptureService& Instance() {
        static CaptureService inst;
        return inst;
    }

    void CaptureAndSend(CServerSocket& ClientSocket, const Cpacket& packet) {
        std::lock_guard<std::mutex> lk(m_mutex);
        try {
            auto [ScreenImagePtr, bitsPtr, nWidth, nHeight, nBitperPixel] = CaptureScreenImage();
            auto& ScreenImage = *ScreenImagePtr;

            // 计算 stride/大小并从像素池获取可复用缓冲，避免每帧分配
            int bytesPerPixel = nBitperPixel / 8;
            if (bytesPerPixel <= 0) bytesPerPixel = 4;
            const int stride = ((nWidth * bytesPerPixel + 31) / 32) * 4;
            size_t expectedSize = static_cast<size_t>(nHeight) * static_cast<size_t>(stride);

            // Acquire buffer from pool
            std::vector<BYTE> currFrame = FrameBufferPool::Instance().Acquire(expectedSize);
            currFrame.resize(expectedSize);

            unsigned int sehCode = 0;
            if (!SEH_Memcpy_NoObj(currFrame.data(), bitsPtr, expectedSize, &sehCode)) {
                std::ostringstream oss;
                oss << "CaptureScreenImage: SEH-protected copy failed in CaptureAndSend, code=0x" << std::hex << sehCode << std::dec;
                std::cerr << oss.str() << std::endl;
                // Release the acquired buffer back and abort this capture
                FrameBufferPool::Instance().Release(std::move(currFrame));
                throw std::runtime_error("Failed to copy pixels from CImage to reusable buffer.");
            }

            // 使用已有的 GenerateScreenData（保持向后兼容）
            std::vector<BYTE> screenData = GenerateScreenData(ScreenImage, m_previousFramePixels, currFrame, nWidth, nHeight, nBitperPixel);

            std::cout << "Screen data size: " << screenData.size() << " bytes" << std::endl;

            Cpacket screenPacket(CMD::CMD_SCREEN_CAPTURE, screenData);
            ClientSocket.SendPacket(screenPacket);

            // 更新上一帧：将旧的 previous 放回池中，然后保存当前帧为新的 previous
            if (!m_previousFramePixels.empty()) {
                FrameBufferPool::Instance().Release(std::move(m_previousFramePixels));
            }
            m_previousFramePixels = std::move(currFrame);
            m_prevWidth = nWidth;
            m_prevHeight = nHeight;
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

private:
    CaptureService() = default;
    std::mutex m_mutex;
    std::vector<BYTE> m_previousFramePixels;
    int m_prevWidth = 0;
    int m_prevHeight = 0;
};

inline void CaptureScreen(CServerSocket& ClientSocket, const Cpacket& packet) {
    CaptureService::Instance().CaptureAndSend(ClientSocket, packet);
}