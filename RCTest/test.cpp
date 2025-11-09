#include "pch.h"
#include <atlimage.h>
#include <gdiplus.h>
#include <cstring>
#include <string>
#include <sstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>

// 包含核心算法
#include "../RemoteControl_Server/include/core/diffAlgorithm.h"
#include "../RemoteControl_Server/include/core/screenCapture.h"
#include "../RemoteControl_Server/include/core/Enities.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

using namespace Gdiplus;

// 全局 GDI+ 初始化
class GdiPlusInitializer {
public:
    static GdiPlusInitializer& GetInstance() {
        static GdiPlusInitializer instance;
        return instance;
    }

private:
    ULONG_PTR m_gdiplusToken;
    GdiplusStartupInput m_gdiplusStartupInput;

    GdiPlusInitializer() : m_gdiplusToken(0) {
        // 初始化 COM
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            throw std::runtime_error("Failed to initialize COM.");
        }

        // 设置 SEH -> C++ 翻译器，这样访问违规（0xC0000005）会被转换为 std::runtime_error，
        // 我们可以使用普通的 C++ try/catch 来捕获并打印诊断信息。
        _set_se_translator([](unsigned int code, _EXCEPTION_POINTERS* ep) {
            std::ostringstream oss;
            oss << "SEH exception code=0x" << std::hex << code;
            throw std::runtime_error(oss.str());
        });

        // 初始化 GDI+
        GdiplusStartup(&m_gdiplusToken, &m_gdiplusStartupInput, nullptr);
    }

    ~GdiPlusInitializer() {
        GdiplusShutdown(m_gdiplusToken);
        CoUninitialize();
    }
};

#include <vector>
#include <tuple>

// 辅助函数：创建一个纯色测试图像
inline CImage CreateTestImage(int width, int height, COLORREF color, int bitsPerPixel = 32) {
    // 确保 GDI+ 已初始化
    GdiPlusInitializer::GetInstance();
    
    CImage testImage;
    HRESULT hr = testImage.Create(width, height, bitsPerPixel);
    if (FAILED(hr)) {
        DWORD lastError = GetLastError();
        std::ostringstream oss;
        oss << "Failed to create test image. hr=0x" << std::hex << hr
            << " lastError=" << std::dec << lastError;
        throw std::runtime_error(oss.str());
    }

    HDC hDC = testImage.GetDC();
    HBRUSH hBrush = CreateSolidBrush(color);
    RECT rect = {0, 0, width, height};
    FillRect(hDC, &rect, hBrush);
    DeleteObject(hBrush);
    testImage.ReleaseDC();

    return testImage;
}

// 辅助函数：从 CImage 获取像素数据
inline std::vector<BYTE> GetImagePixelData(const CImage& image, int width, int height, int bitsPerPixel) {
    int stride = ((width * bitsPerPixel + 31) / 32) * 4;
    const BYTE* pixels = reinterpret_cast<const BYTE*>(image.GetBits());
    return std::vector<BYTE>(pixels, pixels + height * stride);
}

// 辅助函数：在图像上绘制矩形
inline void DrawRectangleOnImage(CImage& image, int x, int y, int width, int height, COLORREF color) {
    HDC hDC = image.GetDC();
    HBRUSH hBrush = CreateSolidBrush(color);
    RECT rect = {x, y, x + width, y + height};
    FillRect(hDC, &rect, hBrush);
    DeleteObject(hBrush);
    image.ReleaseDC();
}

// 辅助函数：解析打包后的前 16 字节头部 (x, y, width, height)
inline std::tuple<int, int, int, int> ParseScreenPacketHeader(const std::vector<BYTE>& data) {
    if (data.size() < sizeof(int) * 4) {
        throw std::runtime_error("Screen packet is too small.");
    }
    int header[4] = {0};
    std::memcpy(header, data.data(), sizeof(header));
    return {header[0], header[1], header[2], header[3]};
}

// 基础用例：首次帧应当返回全屏范围
TEST(DetectScreenDiffTest, ReturnsFullScreenOnFirstFrame) {
    GdiPlusInitializer::GetInstance();

    const int width = 4;
    const int height = 3;
    std::vector<BYTE> previousPixels; // 空向量模拟首次帧
    std::vector<BYTE> currentPixels(width * height * 4, 42);

    auto [minX, minY, maxX, maxY] = DetectScreenDiff(previousPixels, currentPixels, width, height, 4);

    EXPECT_EQ(minX, 0);
    EXPECT_EQ(minY, 0);
    EXPECT_EQ(maxX, width - 1);
    EXPECT_EQ(maxY, height - 1);
}

// 进阶用例：检测单个像素的变化范围
TEST(DetectScreenDiffTest, DetectsSinglePixelChange) {
    GdiPlusInitializer::GetInstance();

    const int width = 5;
    const int height = 4;
    std::vector<BYTE> previousPixels(width * height * 4, 0);
    std::vector<BYTE> currentPixels = previousPixels;

    const int changedX = 2;
    const int changedY = 3;
    const int stride = width * 4;
    const int pixelIndex = changedY * stride + changedX * 4;
    for (int i = 0; i < 4; ++i) {
        currentPixels[pixelIndex + i] = 255;
    }

    auto [minX, minY, maxX, maxY] = DetectScreenDiff(previousPixels, currentPixels, width, height, 4);

    EXPECT_EQ(minX, changedX);
    EXPECT_EQ(minY, changedY);
    EXPECT_EQ(maxX, changedX);
    EXPECT_EQ(maxY, changedY);
}

// 应用用例：验证 CreateScreenData 在全屏更新时的行为
TEST(CreateScreenDataTest, EmitsFullFrameHeaderWhenNoDiffRegion) {
    GdiPlusInitializer::GetInstance();

    const int width = 16;
    const int height = 12;
    const int bitsPerPixel = 32;
    CImage screenImage = CreateTestImage(width, height, RGB(100, 150, 200), bitsPerPixel);

    auto packet = CreateScreenData(screenImage, 0, 0, width - 1, height - 1, width, height, bitsPerPixel);
    auto [x, y, w, h] = ParseScreenPacketHeader(packet);

    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 0);
    EXPECT_EQ(w, width);
    EXPECT_EQ(h, height);
    EXPECT_GT(packet.size(), sizeof(int) * 4);
}

// 应用用例：验证 CreateScreenData 仅封装变化区域
TEST(CreateScreenDataTest, EmitsDiffRegionHeader) {
    GdiPlusInitializer::GetInstance();

    const int width = 20;
    const int height = 18;
    const int bitsPerPixel = 32;
    CImage screenImage = CreateTestImage(width, height, RGB(50, 50, 50), bitsPerPixel);

    const int minX = 5;
    const int minY = 6;
    const int maxX = 11;
    const int maxY = 14;

    DrawRectangleOnImage(screenImage, minX, minY, (maxX - minX + 1), (maxY - minY + 1), RGB(200, 0, 0));

    auto packet = CreateScreenData(screenImage, minX, minY, maxX, maxY, width, height, bitsPerPixel);
    auto [x, y, w, h] = ParseScreenPacketHeader(packet);

    EXPECT_EQ(x, minX);
    EXPECT_EQ(y, minY);
    EXPECT_EQ(w, maxX - minX + 1);
    EXPECT_EQ(h, maxY - minY + 1);
    EXPECT_GT(packet.size(), sizeof(int) * 4);
}

// 综合用例：GenerateScreenData 应沿用差异检测到的范围，并保存图像以供验证
TEST(GenerateScreenDataTest, UsesDetectedDiffBounds) {
    GdiPlusInitializer::GetInstance();

    const int width = 32;
    const int height = 24;
    const int bitsPerPixel = 32;

    // 1. 创建基准图像和变化后的图像
    CImage baselineImage = CreateTestImage(width, height, RGB(10, 10, 10), bitsPerPixel);
    CImage changedImage = CreateTestImage(width, height, RGB(10, 10, 10), bitsPerPixel);
    DrawRectangleOnImage(changedImage, 8, 6, 6, 5, RGB(255, 255, 0)); // 在图像上绘制一个黄色矩形

    // 2. 获取像素数据
    auto previousPixels = GetImagePixelData(baselineImage, width, height, bitsPerPixel);
    auto currentPixels = GetImagePixelData(changedImage, width, height, bitsPerPixel);

    // 3. 理论上预期的差异范围
    auto expected = DetectScreenDiff(previousPixels, currentPixels, width, height, bitsPerPixel / 8);

    // 4. 调用核心函数生成数据包
    std::vector<BYTE> packet;
    try {
        packet = GenerateScreenData(changedImage, previousPixels, currentPixels, width, height, bitsPerPixel);
    }
    catch (const std::exception& ex) {
        FAIL() << "exception in GenerateScreenData: " << ex.what();
    }

    // 5. 验证数据包头部信息是否与预期差异范围一致
    ASSERT_GT(packet.size(), sizeof(int) * 4);
    auto [x, y, w, h] = ParseScreenPacketHeader(packet);

    EXPECT_EQ(x, std::get<0>(expected));
    EXPECT_EQ(y, std::get<1>(expected));
    EXPECT_EQ(w, std::get<2>(expected) - std::get<0>(expected) + 1);
    EXPECT_EQ(h, std::get<3>(expected) - std::get<1>(expected) + 1);

    // 6. 保存图像以供人工检查
    CreateDirectoryA(".\\captured_frames", NULL);
    baselineImage.Save(L".\\captured_frames\\baseline.bmp", Gdiplus::ImageFormatBMP);
    changedImage.Save(L".\\captured_frames\\changed.bmp", Gdiplus::ImageFormatBMP);

    // 7. 从数据包中提取差异图像并保存
    if (packet.size() > sizeof(int) * 4) {
        CImage diffImage;
        diffImage.Create(w, h, bitsPerPixel);
        void* dest_bits = diffImage.GetBits();
        if (dest_bits) {
            std::memcpy(dest_bits, packet.data() + sizeof(int) * 4, packet.size() - sizeof(int) * 4);
        }
        diffImage.Save(L".\\captured_frames\\diff_output.bmp", Gdiplus::ImageFormatBMP);
    }
}

// 实际环境用例：首次屏幕捕获应返回全屏数据包
TEST(ScreenCaptureIntegrationTest, FirstCaptureSendsEntireScreen) {
    GdiPlusInitializer::GetInstance();

    // CaptureScreenImage 可能触发访问冲突，使用 C++ try/catch 捕获（依赖 _set_se_translator）并输出诊断信息
    std::tuple<std::shared_ptr<CImage>, std::vector<BYTE>, int, int, int> captureResult;
    try {
        std::cout << "[DEBUG] Calling CaptureScreenImage()" << std::endl;
        captureResult = CaptureScreenImage();
        std::cout << "[DEBUG] CaptureScreenImage returned" << std::endl;
    } catch (const std::exception& ex) {
        std::cout << "[ERROR] exception in CaptureScreenImage: " << ex.what() << std::endl;
        FAIL() << "exception in CaptureScreenImage: " << ex.what();
    } catch (...) {
        std::cout << "[ERROR] unknown exception in CaptureScreenImage" << std::endl;
        FAIL() << "unknown exception in CaptureScreenImage";
    }

    auto [screenImagePtr, currentPixels, width, height, bitsPerPixel] = captureResult;

    ASSERT_GT(width, 0);
    ASSERT_GT(height, 0);
    ASSERT_GT(bitsPerPixel, 0);
    ASSERT_EQ(static_cast<size_t>(height * ((width * bitsPerPixel + 31) / 32) * 4), currentPixels.size());

    std::vector<BYTE> emptyPreviousFrame;
    auto packet = GenerateScreenData(*screenImagePtr, emptyPreviousFrame, currentPixels, width, height, bitsPerPixel);
    auto [x, y, w, h] = ParseScreenPacketHeader(packet);

    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 0);
    EXPECT_EQ(w, width);
    EXPECT_EQ(h, height);
    EXPECT_GT(packet.size(), sizeof(int) * 4);
}

// 动态差异检测用例：连续捕捉3帧，观察差异检测过程
TEST(ScreenCaptureIntegrationTest, DynamicDiffDetectionMultipleFrames) {
    GdiPlusInitializer::GetInstance();

    std::cout << "\n\n========== [TEST] 开始动态差异检测测试 ==========" << std::endl;

    std::vector<BYTE> prevPixels;
    
    for (int frameIndex = 0; frameIndex < 3; ++frameIndex) {
        std::cout << "\n--- [FRAME " << frameIndex << "] 开始捕捉 ---" << std::endl;
        
        try {
            auto [screenImagePtr, currentPixels, width, height, bitsPerPixel] = CaptureScreenImage();
            
            std::cout << "[FRAME " << frameIndex << "] 捕捉完成: " << width << "x" << height 
                      << " bpp=" << bitsPerPixel << " bytes=" << currentPixels.size() << std::endl;
            
            // 第0帧：prevPixels为空，应该返回全屏
            // 第1-2帧：比较前一帧，检测差异
            if (frameIndex == 0) {
                std::cout << "[FRAME 0] 首帧 - prevPixels为空" << std::endl;
            } else {
                std::cout << "[FRAME " << frameIndex << "] 与前一帧比较..." << std::endl;
            }
            
            std::cout << "[FRAME " << frameIndex << "] 调用 GenerateScreenData..." << std::endl;
            auto packet = GenerateScreenData(*screenImagePtr, prevPixels, currentPixels, width, height, bitsPerPixel);
            
            int x=0, y=0, roiW=0, roiH=0;
            if (packet.size() >= sizeof(int) * 4) {
                std::memcpy(&x, packet.data() + 0, sizeof(int));
                std::memcpy(&y, packet.data() + 4, sizeof(int));
                std::memcpy(&roiW, packet.data() + 8, sizeof(int));
                std::memcpy(&roiH, packet.data() + 12, sizeof(int));
            }
            
            std::cout << "[FRAME " << frameIndex << "] 数据包生成完成:" << std::endl;
            std::cout << "  ROI位置: (" << x << "," << y << ")" << std::endl;
            std::cout << "  ROI大小: " << roiW << "x" << roiH << std::endl;
            std::cout << "  数据包大小: " << packet.size() << " 字节" << std::endl;
            
            if (roiW == 0 && roiH == 0) {
                std::cout << "[FRAME " << frameIndex << "] ⚠️  无差异检测到" << std::endl;
            } else {
                std::cout << "[FRAME " << frameIndex << "] ✓ 检测到差异区域" << std::endl;
            }
            
            // 保存本帧数据供下一帧比较
            prevPixels = std::move(currentPixels);
            
            std::cout << "[FRAME " << frameIndex << "] 完成" << std::endl;
            
            // 帧间间隔，模拟屏幕变化
            if (frameIndex < 2) {
                std::cout << "[FRAME " << frameIndex << "] 等待 500ms，屏幕可能会变化..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        catch (const std::exception& ex) {
            FAIL() << "Frame " << frameIndex << " exception: " << ex.what();
        }
    }
    
    std::cout << "\n========== [TEST] 动态差异检测测试完成 ==========" << std::endl;
}

// 将 PNG 数据写入文件的简易助手
static bool SavePngBytesToFile(const std::wstring& filename, const BYTE* data, size_t size) {
    HANDLE hFile = CreateFileW(filename.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(hFile, data, static_cast<DWORD>(size), &written, NULL);
    CloseHandle(hFile);
    return ok && written == size;
}

// 自定义版本：强制使用自定义差异检测算法（绕过竞争选择）
static std::vector<BYTE> GenerateScreenDataWithCustomAlgorithm(
    const CImage& screenImage, 
    const std::vector<BYTE>& prevPixels, 
    const std::vector<BYTE>& currPixels, 
    int width, 
    int height, 
    int bitPerPixel) {
    
    if (screenImage.IsNull()) {
        std::cerr << "Error: GenerateScreenDataWithCustomAlgorithm received a null CImage object." << std::endl;
        return {};
    }

    std::cout << "=== Using CUSTOM ALGORITHM ONLY ===" << std::endl;
    std::cout << "GenerateScreenDataWithCustomAlgorithm: width=" << width << " height=" << height
              << " prevBytes=" << prevPixels.size() << " currBytes=" << currPixels.size()
              << " bpp=" << bitPerPixel << std::endl;

    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Invalid width/height");
    }
    
    int bytesPerPixel = bitPerPixel / 8;
    if (bytesPerPixel <= 0) bytesPerPixel = 4;
    
    // **关键：直接调用自定义差异检测算法**
    auto [minX, minY, maxX, maxY] = DetectScreenDiff(prevPixels, currPixels, width, height, bytesPerPixel);

    std::cout << "Custom DetectScreenDiff returned: minX=" << minX << " minY=" << minY 
              << " maxX=" << maxX << " maxY=" << maxY << std::endl;

    // 边界检查
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= width) maxX = width - 1;
    if (maxY >= height) maxY = height - 1;

    if (minX > maxX || minY > maxY) {
        minX = 1; minY = 1; maxX = 0; maxY = 0;
        std::cout << "No changes detected by custom algorithm." << std::endl;
    } else {
        int diffW = maxX - minX + 1;
        int diffH = maxY - minY + 1;
        std::cout << "Changes detected! ROI: (" << minX << "," << minY << ") size=" 
                  << diffW << "x" << diffH << std::endl;
    }

    // 调用原始的 CreateScreenData 生成数据包
    return CreateScreenData(screenImage, minX, minY, maxX, maxY, width, height, bitPerPixel);
}

// 从原始像素创建一张 CImage（32bpp）
static CImage CreateImageFromPixels(int w, int h, int bitsPerPixel, const std::vector<BYTE>& pixels) {
    CImage img;
    const int bpp = 32; // 固定为 32bpp 以与 Capture 保持一致
    if (FAILED(img.Create(w, h, bpp))) {
        throw std::runtime_error("CreateImageFromPixels: CImage::Create failed");
    }
    int srcStride = ((w * bitsPerPixel + 31) / 32) * 4;
    int dstStride = ((w * bpp + 31) / 32) * 4;
    BYTE* dst = reinterpret_cast<BYTE*>(img.GetBits());
    if (!dst) throw std::runtime_error("CreateImageFromPixels: GetBits returned null");
    const BYTE* src = pixels.data();
    const size_t rowBytes = static_cast<size_t>(w) * 4; // copy min per row
    for (int y = 0; y < h; ++y) {
        const BYTE* s = src + static_cast<size_t>(y) * srcStride;
        BYTE* d = dst + static_cast<size_t>(y) * dstStride;
        memcpy(d, s, rowBytes);
        if (dstStride > (int)rowBytes) memset(d + rowBytes, 0, dstStride - (int)rowBytes);
    }
    return img;
}

// 在图像上绘制一个矩形边框
static void DrawRectOutline(CImage& img, int x, int y, int w, int h, COLORREF color) {
    HDC hdc = img.GetDC();
    if (!hdc) return;
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    img.ReleaseDC();
}

// 截图模式实现（保存全屏、差异 ROI、以及带框的可视化）
void RunCaptureMode() {
    GdiPlusInitializer::GetInstance();
    // 避免 DPI 虚拟化导致尺寸不一致（可忽略失败）
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        using SetDPIAwareFn = BOOL (WINAPI*)();
        if (auto fn = reinterpret_cast<SetDPIAwareFn>(GetProcAddress(hUser32, "SetProcessDPIAware"))) {
            fn();
        }
    }
    // 使用宽字符串确保与 CImage::Save 使用的 wchar 路径匹配
    const std::wstring outputDirW = L".\\real_captures";
    if (CreateDirectoryW(outputDirW.c_str(), NULL) == 0) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            std::wcerr << L"Failed to create directory: " << outputDirW << L" Error: " << error << std::endl;
            return;
        }
    }

    std::wcout << L"Entering capture mode for 5 seconds..." << std::endl;
    std::wcout << L"Output directory: " << outputDirW << std::endl;

    auto startTime = std::chrono::steady_clock::now();
    int frameCount = 0;

    // 获取 PNG 编码器的 CLSID
    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) == -1) {
        std::wcerr << L"Could not find PNG encoder." << std::endl;
        return;
    }

    // 保存上一帧像素以触发差异检测
    std::vector<BYTE> prevPixels;
    std::cout << "\n\n========== [CAPTURE MODE] 开始5秒实时捕捉和差异检测 ==========" << std::endl;
    
    while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(5)) {
        try {
            std::cout << "\n--- [FRAME " << frameCount << "] 开始捕捉 ---" << std::endl;
            
            // 注意：CaptureScreenImage 返回 shared_ptr<CImage>
            auto [screenImagePtr, pixels, w, h, bpp] = CaptureScreenImage();
            
            std::cout << "[FRAME " << frameCount << "] 捕捉完成: " << w << "x" << h 
                      << " bpp=" << bpp << " bytes=" << pixels.size() << std::endl;

            // 1) 生成差异数据包 - 使用改进的 GenerateScreenData
            std::vector<BYTE> packet;
            try {
                std::cout << "[FRAME " << frameCount << "] 调用 GenerateScreenData..." << std::endl;
                packet = GenerateScreenData(*screenImagePtr, prevPixels, pixels, w, h, bpp);
                std::cout << "[FRAME " << frameCount << "] GenerateScreenData 完成，包大小: " << packet.size() << std::endl;
            } catch (const std::exception& ex) {
                std::cerr << "[FRAME " << frameCount << "] GenerateScreenData error: " << ex.what() << std::endl;
                // 即使失败也继续，避免整个循环中断
                prevPixels = std::move(pixels);
                ++frameCount;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            // 解析头部
            int x=0, y=0, roiW=0, roiH=0;
            if (packet.size() >= sizeof(int) * 4) {
                std::memcpy(&x, packet.data() + 0, sizeof(int));
                std::memcpy(&y, packet.data() + 4, sizeof(int));
                std::memcpy(&roiW, packet.data() + 8, sizeof(int));
                std::memcpy(&roiH, packet.data() + 12, sizeof(int));
            }

            std::cout << "[FRAME " << frameCount << "] ROI: (" << x << "," << y << ") size=" 
                      << roiW << "x" << roiH << std::endl;

            // 2) 保存全屏（使用原始 CImage 指针，避免重建）
            std::wstringstream wssFull;
            wssFull << outputDirW << L"\\capture_" << std::setw(3) << std::setfill(L'0') << frameCount << L".png";
            HRESULT hrFull = screenImagePtr->Save(wssFull.str().c_str(), Gdiplus::ImageFormatPNG);
            if (SUCCEEDED(hrFull)) {
                std::wcout << L"✓ 保存全屏: " << wssFull.str() << std::endl;
            } else {
                std::wcerr << L"✗ 失败保存全屏: " << wssFull.str() << std::endl;
            }

            // 3) 保存差异 ROI PNG（直接写出数据包中的 PNG 字节）
            if (packet.size() > sizeof(int) * 4 && roiW > 0 && roiH > 0) {
                const BYTE* pngData = packet.data() + sizeof(int) * 4;
                const size_t pngSize = packet.size() - sizeof(int) * 4;
                std::wstringstream wssDiff;
                wssDiff << outputDirW << L"\\diff_" << std::setw(3) << std::setfill(L'0') << frameCount << L".png";
                if (SavePngBytesToFile(wssDiff.str(), pngData, pngSize)) {
                    std::wcout << L"✓ 保存差异PNG: " << wssDiff.str() << L" (" << roiW << L"x" << roiH << L")" << std::endl;
                } else {
                    std::wcerr << L"✗ 失败保存差异PNG: " << wssDiff.str() << std::endl;
                }
            } else {
                std::cout << "[FRAME " << frameCount << "] ⚠️ 无差异检测到 (ROI: " << roiW << "x" << roiH << ")" << std::endl;
            }

            // 4) 保存像素数据供下一帧比较
            prevPixels = std::move(pixels);

            // 5) 输出带框可视化（直接在原始 CImage 上绘制红色矩形）
            if (roiW > 0 && roiH > 0) {
                std::cout << "[FRAME " << frameCount << "] 绘制红色矩形边框..." << std::endl;
                DrawRectOutline(*screenImagePtr, x, y, roiW, roiH, RGB(255, 0, 0));
                
                std::wstringstream wssVis;
                wssVis << outputDirW << L"\\vis_" << std::setw(3) << std::setfill(L'0') << frameCount << L".png";
                HRESULT hrVis = screenImagePtr->Save(wssVis.str().c_str(), Gdiplus::ImageFormatPNG);
                if (SUCCEEDED(hrVis)) {
                    std::wcout << L"✓ 保存可视化: " << wssVis.str() << std::endl;
                } else {
                    std::wcerr << L"✗ 失败保存可视化: " << wssVis.str() << std::endl;
                }
            }

            std::cout << "[FRAME " << frameCount << "] 完成处理" << std::endl;
            ++frameCount;
        }
        catch (const std::exception& ex) {
            std::cerr << "[FRAME " << frameCount << "] 异常: " << ex.what() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // ~5 FPS
    }
    
    std::cout << "\n========== [CAPTURE MODE] 捕捉完成，共 " << frameCount << " 帧 ==========" << std::endl;
    std::cout << "输出目录: " << outputDirW.c_str() << std::endl;

    std::wcout << L"Capture finished. Total frames: " << frameCount << std::endl;
}

int main(int argc, char** argv) {
    // 添加诊断日志
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "[MAIN] RCTest.exe 启动" << std::endl;
    std::cout << "[MAIN] 命令行参数个数: " << argc << std::endl;
    for (int i = 0; i < argc; ++i) {
        std::cout << "[MAIN] argv[" << i << "] = " << argv[i] << std::endl;
    }
    std::cout << std::string(80, '=') << std::endl << std::endl;

    if (argc > 1 && std::string(argv[1]) == "--capture") {
        std::cout << "[MAIN] 检测到 --capture 参数，执行 RunCaptureMode()" << std::endl;
        RunCaptureMode();
        return 0;
    }

    // 正常执行 Google Test
    std::cout << "[MAIN] 执行 Google Test 单元测试" << std::endl;
    ::testing::InitGoogleTest(&argc, argv);
    
    // 列出所有可用的测试
    std::cout << "\n[MAIN] 所有注册的测试:" << std::endl;
    std::cout << "[MAIN] 提示：要运行特定测试，使用 --gtest_filter=TestName" << std::endl;
    std::cout << std::string(80, '=') << std::endl << std::endl;
    
    return RUN_ALL_TESTS();
}