#pragma once

#include <vector>
#include <tuple>
#include <cstring>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <comutil.h>
#include <atlimage.h>
#include <iostream>

using BYTE = unsigned char;
using namespace Gdiplus;

struct ScreenDeleter {
    void operator()(HDC hdc) const {
        if (hdc) {
            ::DeleteDC(hdc);
        }
    }
};

using ScopedHDC = std::unique_ptr<std::remove_pointer<HDC>::type, ScreenDeleter>;//RAII

struct StreamDeleter {
    void operator()(IStream* pStream) const {
        if (pStream) {
            pStream->Release();
        }
    }
};

using ScopedStream = std::unique_ptr<IStream, StreamDeleter>;//RAII

class GlobalLockRAII{
public:
   GlobalLockRAII(HGLOBAL hMem) : m_hMem(hMem), m_pData(nullptr){
      if(m_hMem){
           m_pData = ::GlobalLock(m_hMem);
      }
   }
   ~GlobalLockRAII(){
      if(m_pData && m_hMem){
           ::GlobalUnlock(m_hMem);
      }
   }
   // 禁止拷贝
   GlobalLockRAII(const GlobalLockRAII&) = delete;
   GlobalLockRAII& operator=(const GlobalLockRAII&) = delete;

   // 获取裸指针
   void* get() const { return m_pData; }

private:
   HGLOBAL m_hMem;
   void* m_pData;
};

// 自定义差异检测算法：逐像素比较，返回变化矩形的边界
// 返回：minX, minY, maxX, maxY，如果无变化则 minX > maxX
std::tuple<int, int, int, int> DetectScreenDiff(const std::vector<BYTE>& prevPixels, const std::vector<BYTE>& currPixels, int width, int height) {
    if (prevPixels.size() != currPixels.size() || prevPixels.empty()) {
        // 尺寸变化或首次，返回全屏
        return {0, 0, width - 1, height - 1};
    }

    int stride = width * 4; // 假设 32-bit RGBA
    int minX = width, minY = height, maxX = -1, maxY = -1;
    bool hasChanges = false;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = y * stride + x * 4;
            if (std::memcmp(&currPixels[index], &prevPixels[index], 4) != 0) {
                hasChanges = true;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (!hasChanges) {
        // 无变化，返回无效矩形
        return {1, 1, 0, 0}; // minX > maxX
    }

    return {minX, minY, maxX, maxY};
}

// OpenCV 版本的差异检测（需要安装 OpenCV 并链接），完全在本文件内部处理
#ifdef USE_OPENCV
#include <opencv2/opencv.hpp>
static std::tuple<int, int, int, int> DetectScreenDiffOpenCVRaw(
    const std::vector<BYTE>& prevPixels,
    const std::vector<BYTE>& currPixels,
    int width,
    int height) {
    if (prevPixels.size() != currPixels.size() || prevPixels.empty()) {
        return {0, 0, width - 1, height - 1};
    }
    // 从原始 BGRA 像素数据构造 Mat 视图（不拷贝）
    cv::Mat prev(height, width, CV_8UC4, const_cast<BYTE*>(prevPixels.data()));
    cv::Mat curr(height, width, CV_8UC4, const_cast<BYTE*>(currPixels.data()));
    cv::Mat diff, gray, mask;
    cv::absdiff(prev, curr, diff);
    cv::cvtColor(diff, gray, cv::COLOR_BGRA2GRAY);
    // 二值化，阈值为0意味着只要有变化就认为不同
    cv::threshold(gray, mask, 0, 255, cv::THRESH_BINARY);
    std::vector<cv::Point> nzPts;
    cv::findNonZero(mask, nzPts);
    if (nzPts.empty()) {
        return {1, 1, 0, 0}; // 无变化
    }
    cv::Rect rect = cv::boundingRect(nzPts);
    return {rect.x, rect.y, rect.x + rect.width - 1, rect.y + rect.height - 1};
}
#endif

// 竞争策略：测量时间，选择更快的算法
enum Algorithm { CUSTOM, OPENCV };
static Algorithm chosenAlgorithm = CUSTOM; // 默认自定义
static bool tested = false;

// 竞争策略：对相同输入在首次调用时分别计时，之后复用更快的算法
inline std::tuple<int, int, int, int> DetectScreenDiffCompetitive(
    const std::vector<BYTE>& prevPixels,
    const std::vector<BYTE>& currPixels,
    int width,
    int height) {
    if (!tested) {
        // 自定义算法计时
        auto start = std::chrono::high_resolution_clock::now();
        volatile auto r1 = DetectScreenDiff(prevPixels, currPixels, width, height);
        auto end = std::chrono::high_resolution_clock::now();
        auto customTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

#ifdef USE_OPENCV
        // OpenCV 算法计时
        start = std::chrono::high_resolution_clock::now();
        volatile auto r2 = DetectScreenDiffOpenCVRaw(prevPixels, currPixels, width, height);
        end = std::chrono::high_resolution_clock::now();
        auto opencvTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        chosenAlgorithm = (opencvTime < customTime) ? OPENCV : CUSTOM;
#else
        chosenAlgorithm = CUSTOM;
#endif
        tested = true;
    }

    // 使用选择的算法
    if (chosenAlgorithm == CUSTOM) {
        return DetectScreenDiff(prevPixels, currPixels, width, height);
    }
#ifdef USE_OPENCV
    return DetectScreenDiffOpenCVRaw(prevPixels, currPixels, width, height);
#else
    return DetectScreenDiff(prevPixels, currPixels, width, height);
#endif
}

// 辅助函数：创建屏幕数据包
inline std::vector<BYTE> CreateScreenData(const CImage& ScreenImage, int minX, int minY, int maxX, int maxY, int nWidth, int nHeight, int nBitperPixel) {
    std::vector<BYTE> diffData;
    if (minX <= maxX && minY <= maxY) {
        std::cout << "Sending diff image: (" << minX << "," << minY << ") to (" << maxX << "," << maxY << ")" << std::endl;
        // 有变化，发送差异
        int diffWidth = maxX - minX + 1;
        int diffHeight = maxY - minY + 1;

        // 创建差异图像
        CImage diffImage;
        if (diffImage.Create(diffWidth, diffHeight, nBitperPixel) != S_OK) {
            throw std::runtime_error("Failed to create diff image.");
        }

        HDC srcDC = ScreenImage.GetDC();
        HDC dstDC = diffImage.GetDC();
        if (::BitBlt(dstDC, 0, 0, diffWidth, diffHeight, srcDC, minX, minY, SRCCOPY) == FALSE) {
            ScreenImage.ReleaseDC();
            diffImage.ReleaseDC();
            throw std::runtime_error("BitBlt for diff failed.");
        }
        ScreenImage.ReleaseDC();
        diffImage.ReleaseDC();

        // 保存差异图像为 PNG
        IStream* pStreamRaw = nullptr;
        HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &pStreamRaw);
        if (FAILED(hr)) {
            throw std::runtime_error("CreateStreamOnHGlobal failed for diff.");
        }
        ScopedStream pStream(pStreamRaw);

        hr = diffImage.Save(pStream.get(), Gdiplus::ImageFormatPNG);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to save diff image.");
        }

        HGLOBAL hGlobal = nullptr;
        hr = GetHGlobalFromStream(pStream.get(), &hGlobal);
        if (FAILED(hr) || hGlobal == nullptr) {
            throw std::runtime_error("GetHGlobalFromStream failed for diff.");
        }

        GlobalLockRAII lock(hGlobal);
        BYTE* pData = static_cast<BYTE*>(lock.get());
        if (pData == nullptr) {
            throw std::runtime_error("GlobalLock failed for diff.");
        }

        size_t dataSize = ::GlobalSize(hGlobal);
        if (dataSize == 0) {
            throw std::runtime_error("GlobalSize returned zero for diff.");
        }

        // 序列化数据：x, y, w, h + PNG
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&minX), reinterpret_cast<BYTE*>(&minX) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&minY), reinterpret_cast<BYTE*>(&minY) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&diffWidth), reinterpret_cast<BYTE*>(&diffWidth) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&diffHeight), reinterpret_cast<BYTE*>(&diffHeight) + sizeof(int));
        diffData.insert(diffData.end(), pData, pData + dataSize);
    } else {
        std::cout << "Sending full screen image" << std::endl;
        // 无变化或首次，发送全屏
        IStream* pStreamRaw = nullptr;
        HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &pStreamRaw);
        if (FAILED(hr)) {
            throw std::runtime_error("CreateStreamOnHGlobal failed.");
        }
        ScopedStream pStream(pStreamRaw);

        hr = ScreenImage.Save(pStream.get(), Gdiplus::ImageFormatPNG);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to save image to stream.");
        }

        HGLOBAL hGlobal = nullptr;
        hr = GetHGlobalFromStream(pStream.get(), &hGlobal);
        if (FAILED(hr) || hGlobal == nullptr) {
            throw std::runtime_error("GetHGlobalFromStream failed.");
        }

        GlobalLockRAII lock(hGlobal);
        BYTE* pData = static_cast<BYTE*>(lock.get());
        if (pData == nullptr) {
            throw std::runtime_error("GlobalLock failed.");
        }

        size_t dataSize = ::GlobalSize(hGlobal);
        if (dataSize == 0) {
            throw std::runtime_error("GlobalSize returned zero.");
        }

        // 序列化数据：x=0, y=0, w=nWidth, h=nHeight + PNG
        int zero = 0;
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&zero), reinterpret_cast<BYTE*>(&zero) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&zero), reinterpret_cast<BYTE*>(&zero) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&nWidth), reinterpret_cast<BYTE*>(&nWidth) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&nHeight), reinterpret_cast<BYTE*>(&nHeight) + sizeof(int));
        diffData.insert(diffData.end(), pData, pData + dataSize);
    }
    return diffData;
}

// 算法工厂：生成屏幕数据（检测差异并创建数据包）
inline std::vector<BYTE> GenerateScreenData(const CImage& screenImage, const std::vector<BYTE>& prevPixels, const std::vector<BYTE>& currPixels, int width, int height, int bitPerPixel) {
    auto [minX, minY, maxX, maxY] = DetectScreenDiffCompetitive(prevPixels, currPixels, width, height);
    return CreateScreenData(screenImage, minX, minY, maxX, maxY, width, height, bitPerPixel);
}

// 辅助函数：创建屏幕数据包
inline std::vector<BYTE> CreateScreenData(const CImage& ScreenImage, int minX, int minY, int maxX, int maxY, int nWidth, int nHeight, int nBitperPixel) {
    std::vector<BYTE> diffData;
    if (minX <= maxX && minY <= maxY) {
        std::cout << "Sending diff image: (" << minX << "," << minY << ") to (" << maxX << "," << maxY << ")" << std::endl;
        // 有变化，发送差异
        int diffWidth = maxX - minX + 1;
        int diffHeight = maxY - minY + 1;

        // 创建差异图像
        CImage diffImage;
        if (diffImage.Create(diffWidth, diffHeight, nBitperPixel) != S_OK) {
            throw std::runtime_error("Failed to create diff image.");
        }

        HDC srcDC = ScreenImage.GetDC();
        HDC dstDC = diffImage.GetDC();
        if (::BitBlt(dstDC, 0, 0, diffWidth, diffHeight, srcDC, minX, minY, SRCCOPY) == FALSE) {
            ScreenImage.ReleaseDC();
            diffImage.ReleaseDC();
            throw std::runtime_error("BitBlt for diff failed.");
        }
        ScreenImage.ReleaseDC();
        diffImage.ReleaseDC();

        // 保存差异图像为 PNG
        IStream* pStreamRaw = nullptr;
        HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &pStreamRaw);
        if (FAILED(hr)) {
            throw std::runtime_error("CreateStreamOnHGlobal failed for diff.");
        }
        ScopedStream pStream(pStreamRaw);

        hr = diffImage.Save(pStream.get(), Gdiplus::ImageFormatPNG);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to save diff image.");
        }

        HGLOBAL hGlobal = nullptr;
        hr = GetHGlobalFromStream(pStream.get(), &hGlobal);
        if (FAILED(hr) || hGlobal == nullptr) {
            throw std::runtime_error("GetHGlobalFromStream failed for diff.");
        }

        GlobalLockRAII lock(hGlobal);
        BYTE* pData = static_cast<BYTE*>(lock.get());
        if (pData == nullptr) {
            throw std::runtime_error("GlobalLock failed for diff.");
        }

        size_t dataSize = ::GlobalSize(hGlobal);
        if (dataSize == 0) {
            throw std::runtime_error("GlobalSize returned zero for diff.");
        }

        // 序列化数据：x, y, w, h + PNG
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&minX), reinterpret_cast<BYTE*>(&minX) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&minY), reinterpret_cast<BYTE*>(&minY) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&diffWidth), reinterpret_cast<BYTE*>(&diffWidth) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&diffHeight), reinterpret_cast<BYTE*>(&diffHeight) + sizeof(int));
        diffData.insert(diffData.end(), pData, pData + dataSize);
    } else {
        std::cout << "Sending full screen image" << std::endl;
        // 无变化或首次，发送全屏
        IStream* pStreamRaw = nullptr;
        HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &pStreamRaw);
        if (FAILED(hr)) {
            throw std::runtime_error("CreateStreamOnHGlobal failed.");
        }
        ScopedStream pStream(pStreamRaw);

        hr = ScreenImage.Save(pStream.get(), Gdiplus::ImageFormatPNG);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to save image to stream.");
        }

        HGLOBAL hGlobal = nullptr;
        hr = GetHGlobalFromStream(pStream.get(), &hGlobal);
        if (FAILED(hr) || hGlobal == nullptr) {
            throw std::runtime_error("GetHGlobalFromStream failed.");
        }

        GlobalLockRAII lock(hGlobal);
        BYTE* pData = static_cast<BYTE*>(lock.get());
        if (pData == nullptr) {
            throw std::runtime_error("GlobalLock failed.");
        }

        size_t dataSize = ::GlobalSize(hGlobal);
        if (dataSize == 0) {
            throw std::runtime_error("GlobalSize returned zero.");
        }

        // 序列化数据：x=0, y=0, w=nWidth, h=nHeight + PNG
        int zero = 0;
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&zero), reinterpret_cast<BYTE*>(&zero) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&zero), reinterpret_cast<BYTE*>(&zero) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&nWidth), reinterpret_cast<BYTE*>(&nWidth) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&nHeight), reinterpret_cast<BYTE*>(&nHeight) + sizeof(int));
        diffData.insert(diffData.end(), pData, pData + dataSize);
    }
    return diffData;
}

// 算法工厂：生成屏幕数据（检测差异并创建数据包）
inline std::vector<BYTE> GenerateScreenData(const CImage& screenImage, const std::vector<BYTE>& prevPixels, const std::vector<BYTE>& currPixels, int width, int height, int bitPerPixel) {
    auto [minX, minY, maxX, maxY] = DetectScreenDiffCompetitive(prevPixels, currPixels, width, height);
    return CreateScreenData(screenImage, minX, minY, maxX, maxY, width, height, bitPerPixel);
}