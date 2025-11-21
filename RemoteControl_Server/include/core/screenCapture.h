#pragma once

#include <comutil.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "../network/ServerSocket.h"
#include "Enities.h"
#include "diffAlgorithm.h"
#include "pch.h"

using namespace Gdiplus;

// Helper: SEH-protected memcpy implemented in a function without C++ automatic
// objects
#ifdef _MSC_VER
static __declspec(noinline) bool
SEH_Memcpy_NoObj(void *dst, const void *src, size_t size,
                 unsigned int *pExceptionCode) {
  // This function must not contain C++ objects with non-trivial destructors
  __try {
    memcpy(dst, src, size);
    if (pExceptionCode)
      *pExceptionCode = 0;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    if (pExceptionCode)
      *pExceptionCode = ::GetExceptionCode();
    return false;
  }
}
#else
static bool SEH_Memcpy_NoObj(void *dst, const void *src, size_t size,
                             unsigned int *pExceptionCode) {
  try {
    memcpy(dst, src, size);
    if (pExceptionCode)
      *pExceptionCode = 0;
    return true;
  } catch (...) {
    if (pExceptionCode)
      *pExceptionCode = 1;
    return false;
  }
}
#endif

// RAII: window DC acquired by GetDC/ReleaseDC
class ScopedWindowDC {
public:
  explicit ScopedWindowDC(HWND hwnd) : m_hwnd(hwnd), m_hdc(::GetDC(hwnd)) {}
  ~ScopedWindowDC() {
    if (m_hdc) {
      ::ReleaseDC(m_hwnd, m_hdc);
    }
  }
  HDC get() const { return m_hdc; }
  // non-copyable
  ScopedWindowDC(const ScopedWindowDC &) = delete;
  ScopedWindowDC &operator=(const ScopedWindowDC &) = delete;

private:
  HWND m_hwnd{};
  HDC m_hdc{};
};

// 辅助函数：捕获屏幕图像并返回图像和像素数据
inline std::tuple<std::shared_ptr<CImage>, BYTE *, int, int, int>
CaptureScreenImage() {
  // 优先尝试 GetDC(NULL)（在虚拟机中通常更可靠），若失败再尝试 CreateDC("DISPLAY")
  ScopedWindowDC hScreenDC(NULL);
  ScopedHDC hDisplayDC(CreateDC(L"DISPLAY", nullptr, nullptr, nullptr));
  if (hScreenDC.get()) {
    std::cout << "CaptureScreenImage: GetDC(NULL) succeeded, HDC=" << hScreenDC.get()
              << std::endl;
  } else if (hDisplayDC) {
    std::cout << "CaptureScreenImage: GetDC(NULL) failed; CreateDC(\"DISPLAY\") succeeded, HDC="
              << hDisplayDC.get() << std::endl;
  } else {
    std::cout << "CaptureScreenImage: both GetDC(NULL) and CreateDC(\"DISPLAY\") failed" << std::endl;
  }

  // 尝试将进程设置为 DPI 感知，以确保后续的坐标/尺寸为物理像素
  bool dpiAwareSet = false;
  HMODULE hUser32 = ::GetModuleHandleW(L"user32.dll");
  if (hUser32) {
    typedef BOOL(WINAPI * SetProcessDPIAware_t)();
    auto pSetProcessDPIAware = reinterpret_cast<SetProcessDPIAware_t>(
        ::GetProcAddress(hUser32, "SetProcessDPIAware"));
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
    std::cout << "CaptureScreenImage: using primary monitor rc=("
              << mi.rcMonitor.left << "," << mi.rcMonitor.top << ")-("
              << mi.rcMonitor.right << "," << mi.rcMonitor.bottom
              << ") dpiAwareSet=" << (dpiAwareSet ? "Y" : "N") << std::endl;
  } else {
    // 回退到虚拟屏幕（原行为）
    originX = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    originY = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    nWidth = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    nHeight = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
    std::cout << "CaptureScreenImage: GetMonitorInfo failed, falling back to "
                 "virtual screen origin=("
              << originX << "," << originY << ") size=(" << nWidth << ","
              << nHeight << ")" << std::endl;
  }

  // 颜色深度从显示 DC 获取（仅用于日志）
  // 注意：避免在表达式中直接调用 GetDC(NULL) 导致无法释放的 HDC
  // 用于查询设备能力（优先使用 GetDC(NULL)，其次使用 DISPLAY DC）
  HDC tempDCForCaps = hScreenDC.get();
  HDC hScreenDCForCaps = nullptr;
  if (!tempDCForCaps) {
    hScreenDCForCaps = ::GetDC(NULL);
    tempDCForCaps = hScreenDCForCaps;
  }
  const int nBitperPixel = ::GetDeviceCaps(tempDCForCaps, BITSPIXEL);
  std::cout << "CaptureScreenImage: DeviceCaps BITSPIXEL=" << nBitperPixel
            << " tempDCForCaps=" << tempDCForCaps << std::endl;
  if (hScreenDCForCaps) {
    ::ReleaseDC(NULL, hScreenDCForCaps);
  }

  if (nWidth <= 0 || nHeight <= 0) {
    throw std::runtime_error("Invalid virtual screen dimensions.");
  }

  // 为与差异算法保持一致，强制采集为 32bpp
  const int createdBpp = 32;
  // 使用 CreateDIBSection 创建我们自有的 DIB buffer（优于依赖 CImage 内部布局，
  // 在虚拟机驱动下更可靠）
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = nWidth;
  bmi.bmiHeader.biHeight = -nHeight; // top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = static_cast<WORD>(createdBpp);
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biSizeImage = 0;

  void *pvBits = nullptr;
  HBITMAP hDib =
      ::CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
  if (!hDib || !pvBits) {
    DWORD le = GetLastError();
    std::ostringstream oss;
    oss << "CreateDIBSection failed, lastError=" << le;
    throw std::runtime_error(oss.str());
  }

  auto ScreenImage = std::make_shared<CImage>();
  // Attach the DIB to CImage so downstream code using CImage still works
  ScreenImage->Attach(hDib);

  // 使用独立的内存 DC 将屏幕内容 BitBlt 到我们的 DIB
  HDC hMemDC = ::CreateCompatibleDC(NULL);
  if (!hMemDC) {
    ::DeleteObject(hDib);
    throw std::runtime_error("CreateCompatibleDC failed for DIB capture.");
  }
  HGDIOBJ prevObj = ::SelectObject(hMemDC, hDib);

  // 从虚拟屏幕原点开始拷贝整块区域，确保全覆盖
  bool bltOk = false;
  // 优先从 GetDC(NULL) 捕获（在多数 VM 环境下更可靠）
  if (hScreenDC.get()) {
    bltOk = (::BitBlt(hMemDC, 0, 0, nWidth, nHeight, hScreenDC.get(), originX,
                      originY, SRCCOPY) != FALSE);
    std::cout << "CaptureScreenImage: tried BitBlt from GetDC(NULL) -> "
              << (bltOk ? "OK" : "FAILED") << " (srcHDC=" << hScreenDC.get()
              << ", dstHDC=" << hMemDC << ")" << std::endl;
  }
  // 若从 GetDC(NULL) 失败，则尝试 DISPLAY DC
  if (!bltOk && hDisplayDC) {
    bltOk = (::BitBlt(hMemDC, 0, 0, nWidth, nHeight, hDisplayDC.get(), originX,
                      originY, SRCCOPY) != FALSE);
    std::cout << "CaptureScreenImage: tried BitBlt from DISPLAY DC -> "
              << (bltOk ? "OK" : "FAILED") << " (srcHDC=" << hDisplayDC.get()
              << ", dstHDC=" << hMemDC << ")" << std::endl;
  }
  if (!bltOk) {
    // 作为最后候选，再次尝试直接调用 GetDC(NULL)（非常罕见）
    HDC hScreen = ::GetDC(NULL);
    if (hScreen) {
      bool blt2 = (::BitBlt(hMemDC, 0, 0, nWidth, nHeight, hScreen, originX,
                            originY, SRCCOPY) != FALSE);
      std::cout << "CaptureScreenImage: tried BitBlt from GetDC(NULL) (ad-hoc) -> "
                << (blt2 ? "OK" : "FAILED") << " (srcHDC=" << hScreen
                << ", dstHDC=" << hMemDC << ")" << std::endl;
      ::ReleaseDC(NULL, hScreen);
      if (!blt2) {
        ::SelectObject(hMemDC, prevObj);
        ::DeleteDC(hMemDC);
        ScreenImage->Detach();
        ::DeleteObject(hDib);
        throw std::runtime_error("BitBlt failed in CaptureScreenImage with "
                                 "all attempted DCs.");
      }
    } else {
      ::SelectObject(hMemDC, prevObj);
      ::DeleteDC(hMemDC);
      ScreenImage->Detach();
      ::DeleteObject(hDib);
      throw std::runtime_error(
          "CaptureScreenImage: BitBlt failed and GetDC(NULL) returned NULL.");
    }
  }

  // Ensure all GDI operations are flushed
  ::GdiFlush();

  ::SelectObject(hMemDC, prevObj);
  ::DeleteDC(hMemDC);
  std::cout << "CaptureScreenImage: DIB BitBlt done." << std::endl;

  // 动态计算 stride（考虑字节对齐），使用实际创建的位深（固定 32bpp）
  const int stride = ((nWidth * createdBpp + 31) / 32) * 4;
  BYTE *currentPixels = reinterpret_cast<BYTE *>(ScreenImage->GetBits());
  if (!currentPixels) {
    throw std::runtime_error("CaptureScreenImage: GetBits returned nullptr.");
  }
  size_t expectedSize = static_cast<size_t>(nHeight) * stride;
  std::cout << "CaptureScreenImage: stride=" << stride
            << " expectedSize=" << expectedSize
            << " bitsPtr=" << static_cast<void *>(currentPixels) << std::endl;

  // 不再在此处分配并返回像素 vector，改为直接返回指向 CImage 内部像素的指针。
  // 上层调用者可使用 FrameBufferPool
  // 获取缓冲并将数据复制到可复用缓冲中，以避免每帧 heap 分配。
  if (!currentPixels) {
    throw std::runtime_error("CaptureScreenImage: GetBits returned nullptr.");
  }
  return {ScreenImage, currentPixels, nWidth, nHeight, createdBpp};
}

// CaptureService: 持有上一帧状态以避免在函数中使用静态变量
class CaptureService {
public:
  static CaptureService &Instance() {
    static CaptureService inst;
    return inst;
  }

  void CaptureAndSend(CServerSocket &ClientSocket, const Cpacket &packet) {
    std::lock_guard<std::mutex> lk(m_mutex);
    try {
      auto [ScreenImagePtr, bitsPtr, nWidth, nHeight, nBitperPixel] =
          CaptureScreenImage();
      auto &ScreenImage = *ScreenImagePtr;

      // 计算 stride/大小并从像素池获取可复用缓冲，避免每帧分配
      int bytesPerPixel = nBitperPixel / 8;
      if (bytesPerPixel <= 0)
        bytesPerPixel = 4;
      // 修复：按位深（bits）计算以避免在 32bpp 时将 stride 计算成 1/8
      // 错误值。使用 (width * bytesPerPixel * 8) 来表示总位数。
      const int stride = ((nWidth * bytesPerPixel * 8 + 31) / 32) * 4;
      size_t expectedSize =
          static_cast<size_t>(nHeight) * static_cast<size_t>(stride);

      // Acquire buffer from pool
      std::vector<BYTE> currFrame =
          FrameBufferPool::Instance().Acquire(expectedSize);
      currFrame.resize(expectedSize);

      // 在直接 memcpy 之前，先校验源图像实际行字节数（pitch）以避免越界复制
      int srcPitch = ScreenImage.GetPitch();
      size_t srcSize = static_cast<size_t>(std::abs(srcPitch)) *
                       static_cast<size_t>(nHeight);
      size_t copySize = expectedSize;
      std::cout << "CaptureScreenImage: srcPitch=" << srcPitch
                << " srcSize=" << srcSize << " expectedSize=" << expectedSize
                << " bitsPtr=" << static_cast<void *>(bitsPtr) << std::endl;

      if (srcSize < expectedSize) {
        // 源缓冲比预期小，避免直接 memcpy 导致访问冲突。回退到以 CImage 保存为
        // PNG 并发送完整图像。
        std::cerr << "CaptureScreenImage: source buffer smaller than expected, "
                     "falling back to direct image->PNG path."
                  << std::endl;
        // 释放当前获取的池缓冲
        FrameBufferPool::Instance().Release(std::move(currFrame));
        // 直接使用 CreateScreenData 生成并发送（该函数会将 CImage 保存为 PNG）
        std::vector<BYTE> screenData =
            CreateScreenData(ScreenImage, 0, 0, nWidth - 1, nHeight - 1, nWidth,
                             nHeight, nBitperPixel);

        std::cout << "Screen data size (fallback PNG): " << screenData.size()
                  << " bytes" << std::endl;
        Cpacket screenPacket(CMD::CMD_SCREEN_CAPTURE, screenData);
        ClientSocket.SendPacket(screenPacket);

        // 更新上一帧为 empty（上次帧保持不变），后续帧仍可继续
        return;
      }

      // 仅在源大小充足时进行复制。优先使用 GetDIBits（兼容虚拟机驱动），
      // 失败时再回退到受 SEH 保护的 memcpy。
      bool copied = false;
      // 尝试通过获取 CImage 的位图句柄并使用 GetDIBits 填充复用缓冲
      do {
        // 强制刷新任何挂起的 GDI 操作以减少驱动延迟问题
        ::GdiFlush();
        HBITMAP hBmp = nullptr;
        HDC hTmpDC = nullptr;
        // 获取当前 CImage 中选中的位图句柄（需要在 GetDC/ReleaseDC 之后重新获取
        // DC）
        HDC hImgDC = ScreenImage.GetDC();
        if (hImgDC) {
          hBmp =
              reinterpret_cast<HBITMAP>(::GetCurrentObject(hImgDC, OBJ_BITMAP));
          ScreenImage.ReleaseDC();
        }

        if (hBmp) {
          // 准备 BITMAPINFO（top-down，32bpp）
          BITMAPINFO bmi = {};
          bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
          bmi.bmiHeader.biWidth = nWidth;
          bmi.bmiHeader.biHeight = -nHeight; // top-down
          bmi.bmiHeader.biPlanes = 1;
          bmi.bmiHeader.biBitCount = static_cast<WORD>(bytesPerPixel * 8);
          bmi.bmiHeader.biCompression = BI_RGB;
          bmi.bmiHeader.biSizeImage = static_cast<DWORD>(expectedSize);

          hTmpDC = ::CreateCompatibleDC(NULL);
          if (hTmpDC) {
            // 将像素直接写入复用缓冲
            int ret = ::GetDIBits(hTmpDC, hBmp, 0, nHeight, currFrame.data(),
                                  &bmi, DIB_RGB_COLORS);
            ::DeleteDC(hTmpDC);
            if (ret == nHeight) {
              copied = true;
            } else {
              std::cerr << "CaptureScreenImage: GetDIBits returned " << ret
                        << " expected=" << nHeight << std::endl;
            }
          }
        }
        if (!copied)
          break; // 如果 GetDIBits 未成功则跳出并回退到 memcpy
      } while (false);

      if (!copied) {
        // 退回到原来的 SEH 保护 memcpy（保持向后兼容）
        copySize = expectedSize;
        unsigned int sehCode = 0;
        if (!SEH_Memcpy_NoObj(currFrame.data(), bitsPtr, copySize, &sehCode)) {
          std::ostringstream oss;
          oss << "CaptureScreenImage: SEH-protected copy failed in "
                 "CaptureAndSend, code=0x"
              << std::hex << sehCode << std::dec;
          std::cerr << oss.str() << std::endl;
          // Release the acquired buffer back and abort this capture
          FrameBufferPool::Instance().Release(std::move(currFrame));
          throw std::runtime_error(
              "Failed to copy pixels from CImage to reusable buffer.");
        }
      }

      // 使用已有的 GenerateScreenData（保持向后兼容）
      std::vector<BYTE> screenData =
          GenerateScreenData(ScreenImage, m_previousFramePixels, currFrame,
                             nWidth, nHeight, nBitperPixel);

      std::cout << "Screen data size: " << screenData.size() << " bytes"
                << std::endl;

      Cpacket screenPacket(CMD::CMD_SCREEN_CAPTURE, screenData);
      ClientSocket.SendPacket(screenPacket);

      // 更新上一帧：将旧的 previous 放回池中，然后保存当前帧为新的 previous
      if (!m_previousFramePixels.empty()) {
        FrameBufferPool::Instance().Release(std::move(m_previousFramePixels));
      }
      m_previousFramePixels = std::move(currFrame);
      m_prevWidth = nWidth;
      m_prevHeight = nHeight;
    } catch (const std::exception &ex) {
      std::string errMsg = "Exception caught: " + std::string(ex.what());
      ClientSocket.SendErrorPacket(errMsg);
      return;
    } catch (...) {
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

inline void CaptureScreen(CServerSocket &ClientSocket, const Cpacket &packet) {
  CaptureService::Instance().CaptureAndSend(ClientSocket, packet);
}