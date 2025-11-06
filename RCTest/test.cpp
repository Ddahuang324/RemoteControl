#include "pch.h"
#include <atlimage.h>
#include <gdiplus.h>
#include <chrono>
#include <thread>

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
        GdiplusStartup(&m_gdiplusToken, &m_gdiplusStartupInput, nullptr);
    }

    ~GdiPlusInitializer() {
        GdiplusShutdown(m_gdiplusToken);
    }
};

// 辅助函数：创建一个纯色测试图像
inline CImage CreateTestImage(int width, int height, COLORREF color, int bitsPerPixel = 32) {
    CImage testImage;
    if (testImage.Create(width, height, bitsPerPixel) != S_OK) {
        throw std::runtime_error("Failed to create test image.");
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
    BYTE* pixels = reinterpret_cast<BYTE*>(image.GetBits());
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

// 测试用例 1: 检测屏幕差异算法
TEST(ScreenCaptureTest, DetectScreenDiffBasic) {
    GdiPlusInitializer::GetInstance();

    int width = 1920;
    int height = 1080;
    int bitsPerPixel = 32;
    int stride = ((width * bitsPerPixel + 31) / 32) * 4;

    // 创建两个相同的图像
    CImage image1 = CreateTestImage(width, height, RGB(255, 255, 255), bitsPerPixel);
    CImage image2 = CreateTestImage(width, height, RGB(255, 255, 255), bitsPerPixel);

    auto pixels1 = GetImagePixelData(image1, width, height, bitsPerPixel);
    auto pixels2 = GetImagePixelData(image2, width, height, bitsPerPixel);

    // 测试：无变化
    auto [minX, minY, maxX, maxY] = DetectScreenDiff(pixels1, pixels2, width, height);
    EXPECT_GT(minX, maxX) << "Expected no changes (minX > maxX)";
}

// 测试用例 2: 检测屏幕差异 - 有变化
TEST(ScreenCaptureTest, DetectScreenDiffWithChanges) {
    GdiPlusInitializer::GetInstance();

    int width = 1920;
    int height = 1080;
    int bitsPerPixel = 32;

    // 创建第一个图像（白色）
    CImage image1 = CreateTestImage(width, height, RGB(255, 255, 255), bitsPerPixel);
    auto pixels1 = GetImagePixelData(image1, width, height, bitsPerPixel);

    // 创建第二个图像并修改一部分（模拟变化）
    CImage image2 = CreateTestImage(width, height, RGB(255, 255, 255), bitsPerPixel);
    DrawRectangleOnImage(image2, 100, 100, 200, 200, RGB(255, 0, 0));  // 绘制红色矩形
    auto pixels2 = GetImagePixelData(image2, width, height, bitsPerPixel);

    // 测试：检测到变化
    auto [minX, minY, maxX, maxY] = DetectScreenDiff(pixels1, pixels2, width, height);
    EXPECT_LE(minX, maxX) << "Expected changes to be detected (minX <= maxX)";
    EXPECT_GE(minX, 90) << "Change should be near x=100";
    EXPECT_LE(maxX, 310) << "Change should be near x=300";
}

// 测试用例 3: 首次捕捉（尺寸变化）
TEST(ScreenCaptureTest, DetectScreenDiffSizeChange) {
    GdiPlusInitializer::GetInstance();

    int width1 = 1920;
    int height1 = 1080;
    int width2 = 1600;
    int height2 = 900;
    int bitsPerPixel = 32;

    CImage image1 = CreateTestImage(width1, height1, RGB(255, 255, 255), bitsPerPixel);
    auto pixels1 = GetImagePixelData(image1, width1, height1, bitsPerPixel);

    CImage image2 = CreateTestImage(width2, height2, RGB(255, 255, 255), bitsPerPixel);
    auto pixels2 = GetImagePixelData(image2, width2, height2, bitsPerPixel);

    // 尺寸不同，应该返回全屏
    auto [minX, minY, maxX, maxY] = DetectScreenDiff(pixels1, pixels2, width2, height2);
    EXPECT_EQ(minX, 0) << "Should return full screen when size changes";
    EXPECT_EQ(minY, 0) << "Should return full screen when size changes";
}

// 测试用例 4: 创建屏幕数据包
TEST(ScreenCaptureTest, CreateScreenDataPacket) {
    GdiPlusInitializer::GetInstance();

    int width = 1920;
    int height = 1080;
    int bitsPerPixel = 32;

    CImage image = CreateTestImage(width, height, RGB(255, 255, 255), bitsPerPixel);

    // 测试创建全屏数据包
    std::vector<BYTE> screenData = CreateScreenData(
        image, 
        0, 0, width - 1, height - 1,  // 无效矩形（无变化）
        width, height, bitsPerPixel
    );

    // 验证数据包结构: x(4字节) + y(4字节) + w(4字节) + h(4字节) + PNG数据
    ASSERT_GT(screenData.size(), 16) << "Screen data should contain at least header";
    
    // 提取头部信息
    int x = *reinterpret_cast<int*>(&screenData[0]);
    int y = *reinterpret_cast<int*>(&screenData[4]);
    int w = *reinterpret_cast<int*>(&screenData[8]);
    int h = *reinterpret_cast<int*>(&screenData[12]);

    EXPECT_EQ(x, 0) << "Full screen should start at (0, 0)";
    EXPECT_EQ(y, 0) << "Full screen should start at (0, 0)";
    EXPECT_EQ(w, width) << "Full screen width should match";
    EXPECT_EQ(h, height) << "Full screen height should match";
}

// 测试用例 5: 差异数据包创建
TEST(ScreenCaptureTest, CreateDiffDataPacket) {
    GdiPlusInitializer::GetInstance();

    int width = 1920;
    int height = 1080;
    int bitsPerPixel = 32;

    CImage image = CreateTestImage(width, height, RGB(255, 255, 255), bitsPerPixel);
    DrawRectangleOnImage(image, 500, 500, 400, 300, RGB(0, 255, 0));

    // 测试创建差异区域数据包
    std::vector<BYTE> screenData = CreateScreenData(
        image,
        500, 500, 899, 799,  // 差异矩形
        width, height, bitsPerPixel
    );

    // 验证数据包结构
    ASSERT_GT(screenData.size(), 16) << "Diff data should contain header";

    int x = *reinterpret_cast<int*>(&screenData[0]);
    int y = *reinterpret_cast<int*>(&screenData[4]);
    int w = *reinterpret_cast<int*>(&screenData[8]);
    int h = *reinterpret_cast<int*>(&screenData[12]);

    EXPECT_EQ(x, 500) << "Diff should start at correct X";
    EXPECT_EQ(y, 500) << "Diff should start at correct Y";
    EXPECT_EQ(w, 400) << "Diff width should match";
    EXPECT_EQ(h, 300) << "Diff height should match";
}

// 测试用例 6: 生成屏幕数据工厂函数
TEST(ScreenCaptureTest, GenerateScreenDataFactory) {
    GdiPlusInitializer::GetInstance();

    int width = 1920;
    int height = 1080;
    int bitsPerPixel = 32;

    CImage image1 = CreateTestImage(width, height, RGB(255, 255, 255), bitsPerPixel);
    auto pixels1 = GetImagePixelData(image1, width, height, bitsPerPixel);

    CImage image2 = CreateTestImage(width, height, RGB(255, 255, 255), bitsPerPixel);
    DrawRectangleOnImage(image2, 100, 100, 200, 200, RGB(255, 0, 0));
    auto pixels2 = GetImagePixelData(image2, width, height, bitsPerPixel);

    // 使用工厂函数生成数据
    std::vector<BYTE> screenData = GenerateScreenData(image2, pixels1, pixels2, width, height, bitsPerPixel);

    // 验证数据包有效性
    EXPECT_GT(screenData.size(), 16) << "Generated screen data should be valid";
}

// 测试用例 7: 屏幕捕捉循环模拟
TEST(ScreenCaptureTest, ScreenCaptureLoopSimulation) {
    GdiPlusInitializer::GetInstance();

    int width = 1920;
    int height = 1080;
    int bitsPerPixel = 32;

    std::vector<std::vector<BYTE>> capturedFrames;
    static std::vector<BYTE> previousFramePixels;
    static int prevWidth = 0, prevHeight = 0;

    // 模拟 5 帧的捕捉
    for (int frame = 0; frame < 5; ++frame) {
        CImage currentImage = CreateTestImage(width, height, RGB(255, 255, 255), bitsPerPixel);

        // 每隔一帧在不同位置画一个矩形
        if (frame % 2 == 0) {
            DrawRectangleOnImage(currentImage, 200 + frame * 50, 200, 100, 100, RGB(0, 0, 255));
        }

        auto currentFramePixels = GetImagePixelData(currentImage, width, height, bitsPerPixel);

        // 执行差异检测
        auto [minX, minY, maxX, maxY] = DetectScreenDiff(previousFramePixels, currentFramePixels, width, height);

        // 生成数据包
        std::vector<BYTE> screenData = CreateScreenData(
            currentImage,
            minX, minY, maxX, maxY,
            width, height, bitsPerPixel
        );

        capturedFrames.push_back(screenData);

        // 更新前一帧
        previousFramePixels = std::move(currentFramePixels);
        prevWidth = width;
        prevHeight = height;

        std::cout << "Frame " << frame << " captured, data size: " << screenData.size() << " bytes" << std::endl;
    }

    // 验证捕捉的帧数
    EXPECT_EQ(capturedFrames.size(), 5) << "Should capture 5 frames";

    // 验证每个帧都有数据
    for (size_t i = 0; i < capturedFrames.size(); ++i) {
        EXPECT_GT(capturedFrames[i].size(), 16) << "Frame " << i << " should have valid data";
    }
}

// 测试用例 8: 性能测试 - 差异检测性能
TEST(ScreenCaptureTest, PerformanceTestDiffDetection) {
    GdiPlusInitializer::GetInstance();

    int width = 1920;
    int height = 1080;
    int bitsPerPixel = 32;

    CImage image1 = CreateTestImage(width, height, RGB(255, 255, 255), bitsPerPixel);
    auto pixels1 = GetImagePixelData(image1, width, height, bitsPerPixel);

    CImage image2 = CreateTestImage(width, height, RGB(255, 255, 255), bitsPerPixel);
    DrawRectangleOnImage(image2, 500, 500, 300, 300, RGB(255, 0, 0));
    auto pixels2 = GetImagePixelData(image2, width, height, bitsPerPixel);

    // 性能测试：多次调用计时
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        volatile auto result = DetectScreenDiffCompetitive(pixels1, pixels2, width, height);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "100 iterations of diff detection took: " << duration << " ms" << std::endl;
    
    // 验证性能在可接受范围内（100次应该在1秒以内）
    EXPECT_LT(duration, 1000) << "Diff detection should be fast";
}

// 测试用例 9: 多种颜色变化检测
TEST(ScreenCaptureTest, MultiColorChangeDetection) {
    GdiPlusInitializer::GetInstance();

    int width = 1920;
    int height = 1080;
    int bitsPerPixel = 32;

    CImage image1 = CreateTestImage(width, height, RGB(200, 200, 200), bitsPerPixel);
    auto pixels1 = GetImagePixelData(image1, width, height, bitsPerPixel);

    CImage image2 = CreateTestImage(width, height, RGB(200, 200, 200), bitsPerPixel);
    
    // 在不同位置画多个矩形
    DrawRectangleOnImage(image2, 100, 100, 100, 100, RGB(255, 0, 0));    // 红色
    DrawRectangleOnImage(image2, 400, 100, 100, 100, RGB(0, 255, 0));    // 绿色
    DrawRectangleOnImage(image2, 700, 100, 100, 100, RGB(0, 0, 255));    // 蓝色

    auto pixels2 = GetImagePixelData(image2, width, height, bitsPerPixel);

    auto [minX, minY, maxX, maxY] = DetectScreenDiff(pixels1, pixels2, width, height);
    
    // 应该检测到变化并包含所有修改区域
    EXPECT_LE(minX, 100) << "Should detect leftmost change";
    EXPECT_GE(maxX, 800) << "Should detect rightmost change";
}