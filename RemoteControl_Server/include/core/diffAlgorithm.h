#pragma once

#define USE_OPENCV 1

#include <gdiplus.h>
#include <vector>
#include <tuple>
#include <cstring>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <comutil.h>
#include <atlimage.h>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <mutex>

using BYTE = unsigned char;
using namespace Gdiplus;

// 前置声明：避免在类定义中引用后面定义的自由函数/静态函数导致编译错误
std::tuple<int,int,int,int> DetectScreenDiff(const std::vector<BYTE>& prevPixels, const std::vector<BYTE>& currPixels, int width, int height, int bytesPerPixel);
static void SaveCImageToStreamFallback(const CImage& img, IStream* pStream);

// -------------------------
// Object-oriented wrappers (轻量实现，基于现有自由函数以保持行为不变)
// -------------------------

// 简单的线程安全缓冲池，用作像素缓冲复用
class FrameBufferPool {
public:
    static FrameBufferPool& Instance() {
        static FrameBufferPool inst;
        return inst;
    }

    std::vector<BYTE> Acquire(size_t size) {
        std::lock_guard<std::mutex> lk(m_mutex);
        for (auto it = m_pool.begin(); it != m_pool.end(); ++it) {
            if (it->capacity() >= size) {
                std::vector<BYTE> buf = std::move(*it);
                m_pool.erase(it);
                buf.clear();
                buf.reserve(size);
                return buf;
            }
        }
        std::vector<BYTE> buf;
        buf.reserve(size);
        return buf;
    }

    void Release(std::vector<BYTE>&& buf) {
        std::lock_guard<std::mutex> lk(m_mutex);
        buf.clear();
        if (m_pool.size() < m_maxPoolSize) m_pool.emplace_back(std::move(buf));
    }

private:
    FrameBufferPool() = default;
    std::mutex m_mutex;
    std::vector<std::vector<BYTE>> m_pool;
    const size_t m_maxPoolSize = 8;
};

// Diff 引擎外观，封装选择逻辑
class DiffEngine {
public:
    static DiffEngine& Instance() {
        static DiffEngine inst;
        return inst;
    }
    std::tuple<int,int,int,int> Detect(const std::vector<BYTE>& prev, const std::vector<BYTE>& curr, int width, int height, int bytesPerPixel) const {
        // 调用已存在的 DetectScreenDiff（自定义逐像素实现）以避免对尚未声明的竞争函数依赖
        return DetectScreenDiff(prev, curr, width, height, bytesPerPixel);
    }
private:
    DiffEngine() = default;
};

// Image 编码外观，封装 PNG 编码入口
class ImageCodec {
public:
    static ImageCodec& Instance() {
        static ImageCodec inst;
        return inst;
    }
    std::vector<BYTE> ToPNG(const CImage& img) {
        // 使用现有的 SaveCImageToStreamFallback 将 CImage 写入 IStream，然后用 WinAPI 直接锁定 HGLOBAL 提取字节
        IStream* pStreamRaw = nullptr;
        HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &pStreamRaw);
        if (FAILED(hr) || pStreamRaw == nullptr) {
            throw std::runtime_error("ImageCodec::ToPNG: CreateStreamOnHGlobal failed.");
        }

        // 直接使用原始指针，避免依赖后面才定义的 ScopedStream
        SaveCImageToStreamFallback(img, pStreamRaw);

        HGLOBAL hGlobal = nullptr;
        hr = GetHGlobalFromStream(pStreamRaw, &hGlobal);
        if (FAILED(hr) || hGlobal == nullptr) {
            pStreamRaw->Release();
            throw std::runtime_error("ImageCodec::ToPNG: GetHGlobalFromStream failed.");
        }

        BYTE* pData = static_cast<BYTE*>(::GlobalLock(hGlobal));
        if (pData == nullptr) {
            pStreamRaw->Release();
            throw std::runtime_error("ImageCodec::ToPNG: GlobalLock returned NULL.");
        }
        SIZE_T dataSize = ::GlobalSize(hGlobal);
        if (dataSize == 0) {
            ::GlobalUnlock(hGlobal);
            pStreamRaw->Release();
            throw std::runtime_error("ImageCodec::ToPNG: GlobalSize returned zero.");
        }
        std::vector<BYTE> out(pData, pData + static_cast<size_t>(dataSize));
        ::GlobalUnlock(hGlobal);
        pStreamRaw->Release();
        return out;
    }
private:
    ImageCodec() = default;
};

// 序列化工具：用于把 ROI/尺寸 + 编码数据合并成最终字节包
class ScreenSerializer {
public:
    static std::vector<BYTE> FromFullImage(const std::vector<BYTE>& pngBytes, int width, int height) {
        std::vector<BYTE> out;
        int zero = 0;
        out.insert(out.end(), reinterpret_cast<const BYTE*>(&zero), reinterpret_cast<const BYTE*>(&zero) + sizeof(int));
        out.insert(out.end(), reinterpret_cast<const BYTE*>(&zero), reinterpret_cast<const BYTE*>(&zero) + sizeof(int));
        out.insert(out.end(), reinterpret_cast<const BYTE*>(&width), reinterpret_cast<const BYTE*>(&width) + sizeof(int));
        out.insert(out.end(), reinterpret_cast<const BYTE*>(&height), reinterpret_cast<const BYTE*>(&height) + sizeof(int));
        out.insert(out.end(), pngBytes.begin(), pngBytes.end());
        return out;
    }

    static std::vector<BYTE> FromDiff(int x, int y, int w, int h, const std::vector<BYTE>& pngBytes) {
        std::vector<BYTE> out;
        out.insert(out.end(), reinterpret_cast<const BYTE*>(&x), reinterpret_cast<const BYTE*>(&x) + sizeof(int));
        out.insert(out.end(), reinterpret_cast<const BYTE*>(&y), reinterpret_cast<const BYTE*>(&y) + sizeof(int));
        out.insert(out.end(), reinterpret_cast<const BYTE*>(&w), reinterpret_cast<const BYTE*>(&w) + sizeof(int));
        out.insert(out.end(), reinterpret_cast<const BYTE*>(&h), reinterpret_cast<const BYTE*>(&h) + sizeof(int));
        out.insert(out.end(), pngBytes.begin(), pngBytes.end());
        return out;
    }
};


// Helper: get encoder CLSID for a given mime type (e.g., "image/png")
// 修复：C2264 错误通常是因为函数声明和定义不一致，或未声明。建议将 static int GetEncoderClsid(...) 的声明提前到文件顶部，并确保声明和定义一致。
static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;          // number of image encoders
    UINT size = 0;         // size of the image encoder array in bytes

    ImageCodecInfo* pImageCodecInfo = nullptr;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;  // Failure

    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == nullptr) return -1;  // Failure

    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j; // success
        }
    }
    free(pImageCodecInfo);
    return -1; // not found
}

// Helper: try to save a CImage into a IStream using GDI+ Bitmap encoder if necessary
static void SaveCImageToStreamFallback(const CImage& img, IStream* pStream) {
    // First try CImage::Save (some formats succeed)
    HRESULT hr = img.Save(pStream, Gdiplus::ImageFormatPNG);
    if (SUCCEEDED(hr)) return;

    // Fallback: convert to HBITMAP -> Gdiplus::Bitmap and save via encoder CLSID
        // Fallback: create a Gdiplus::Bitmap and copy pixels via LockBits, then save
        int w = img.GetWidth();
        int h = img.GetHeight();
        int srcBpp = img.GetBPP();
        const BYTE* srcBits = reinterpret_cast<const BYTE*>(img.GetBits());
        if (!srcBits) {
            throw std::runtime_error("SaveCImageToStreamFallback: img.GetBits() returned NULL.");
        }

        // Create destination bitmap with 32bpp ARGB
        Bitmap dstBitmap(w, h, PixelFormat32bppARGB);
        BitmapData bd;
        Rect rect(0, 0, w, h);
        Status st = dstBitmap.LockBits(&rect, ImageLockModeWrite, PixelFormat32bppARGB, &bd);
        if (st != Ok) {
            std::ostringstream oss;
            oss << "SaveCImageToStreamFallback: LockBits failed status=" << static_cast<int>(st);
            throw std::runtime_error(oss.str());
        }

        int srcStride = ((w * srcBpp + 31) / 32) * 4;
        BYTE* dstBase = static_cast<BYTE*>(bd.Scan0);
        int dstStride = bd.Stride;
        // bd.Stride may be negative; handle accordingly
        for (int y = 0; y < h; ++y) {
            BYTE* dstRow;
            if (dstStride > 0) dstRow = dstBase + static_cast<INT_PTR>(y) * dstStride;
            else dstRow = dstBase + static_cast<INT_PTR>((h - 1 - y)) * (-dstStride);

            const BYTE* srcRow = srcBits + static_cast<INT_PTR>(y) * srcStride;

            // Copy min number of bytes per row (dst expects 4*w)
            size_t copyBytes = static_cast<size_t>(w) * 4;
            // If srcStride is smaller for some reason, clamp
            if (copyBytes > static_cast<size_t>(srcStride)) copyBytes = srcStride;

            memcpy(dstRow, srcRow, copyBytes);
            // If dstStride > copyBytes, zero the rest to be safe
            if (static_cast<size_t>(abs(dstStride)) > copyBytes) {
                memset(dstRow + copyBytes, 0, static_cast<size_t>(abs(dstStride)) - copyBytes);
            }
        }

        dstBitmap.UnlockBits(&bd);

        CLSID pngClsid;
        if (GetEncoderClsid(L"image/png", &pngClsid) < 0) {
            throw std::runtime_error("SaveCImageToStreamFallback: PNG encoder not found.");
        }

        st = dstBitmap.Save(pStream, &pngClsid, NULL);
        if (st != Ok) {
            std::ostringstream oss;
            oss << "SaveCImageToStreamFallback: GDI+ Bitmap::Save failed status=" << static_cast<int>(st);
            throw std::runtime_error(oss.str());
        }
}

struct ScreenDeleter {
    void operator()(HDC hdc) const {
        if (hdc) {
            ::DeleteDC(hdc);
        }
    }
};

using ScopedHDC = std::unique_ptr<std::remove_pointer<HDC>::type, ScreenDeleter>; // RAII

struct StreamDeleter {
    void operator()(IStream* pStream) const {
        if (pStream) {
            pStream->Release();
        }
    }
};

using ScopedStream = std::unique_ptr<IStream, StreamDeleter>; // RAII

class GlobalLockRAII {
public:
    explicit GlobalLockRAII(HGLOBAL hMem) : m_hMem(hMem), m_pData(nullptr) {
        if (m_hMem) {
            m_pData = ::GlobalLock(m_hMem);
        }
    }
    ~GlobalLockRAII() {
        if (m_pData && m_hMem) {
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

// RAII wrapper for CImage GetDC/ReleaseDC to ensure paired release even on exceptions
class CImageDCRAII {
public:
    explicit CImageDCRAII(const CImage& img) : m_img(img), m_hdc(NULL), m_acquired(false) {
        // Some CImage implementations have non-const GetDC; use const_cast to call safely
        m_hdc = const_cast<CImage&>(m_img).GetDC();
        if (m_hdc) m_acquired = true;
    }
    ~CImageDCRAII() {
        if (m_acquired) {
            const_cast<CImage&>(m_img).ReleaseDC();
        }
    }
    CImageDCRAII(const CImageDCRAII&) = delete;
    CImageDCRAII& operator=(const CImageDCRAII&) = delete;

    HDC get() const { return m_hdc; }
    explicit operator bool() const { return m_acquired; }

private:
    const CImage& m_img;
    HDC m_hdc;
    bool m_acquired;
};














// 自定义差异检测算法：逐像素比较，返回变化矩形的边界
// 返回：minX, minY, maxX, maxY，如果无变化则 minX > maxX
std::tuple<int, int, int, int> DetectScreenDiff(const std::vector<BYTE>& prevPixels, const std::vector<BYTE>& currPixels, int width, int height, int bytesPerPixel) {
    std::cout << "[DetectScreenDiff] ENTRY: width=" << width << " height=" << height << " bytesPerPixel=" << bytesPerPixel
              << " prevSize=" << prevPixels.size() << " currSize=" << currPixels.size() << std::endl;
    
    if (prevPixels.empty() || currPixels.empty() || prevPixels.size() != currPixels.size()) {
        // 尺寸变化或首次，返回全屏
        std::cout << "[DetectScreenDiff] First frame or size mismatch - returning full screen (0,0)-(" 
                  << (width-1) << "," << (height-1) << ")" << std::endl;
        return {0, 0, width - 1, height - 1};
    }

    // Use same stride calculation as used when pixel buffer is created:
    // rows are DWORD-aligned. This avoids mis-indexing when each row has padding.
    int stride = ((width * bytesPerPixel + 31) / 32) * 4;
    std::cout << "[DetectScreenDiff] stride=" << stride << " (bytesPerPixel=" << bytesPerPixel << ")" << std::endl;
    
    int minX = width, minY = height, maxX = -1, maxY = -1;
    bool hasChanges = false;
    int changeCount = 0;

    for (int y = 0; y < height; ++y) {
        size_t rowBase = static_cast<size_t>(y) * static_cast<size_t>(stride);
        for (int x = 0; x < width; ++x) {
            size_t index = rowBase + static_cast<size_t>(x) * static_cast<size_t>(bytesPerPixel);
            // 额外安全检查
            if (index + bytesPerPixel > currPixels.size() || index + bytesPerPixel > prevPixels.size()) {
                // 非法索引，防止越界
                continue;
            }
            if (std::memcmp(&currPixels[index], &prevPixels[index], bytesPerPixel) != 0) {
                hasChanges = true;
                changeCount++;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (!hasChanges) {
        // 无变化，返回无效矩形
        std::cout << "[DetectScreenDiff] No changes detected - returning invalid rect (1,1,0,0)" << std::endl;
        return {1, 1, 0, 0}; // minX > maxX
    }

    std::cout << "[DetectScreenDiff] Changes detected! changeCount=" << changeCount 
              << " rect=(" << minX << "," << minY << ")-(" << maxX << "," << maxY << ")" << std::endl;
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

// 竞争策略：测量时间，选择更快的算法（简化生命周期：仅在首次调用时选择并缓存于函数静态变量中）
enum class Algorithm { Custom, OpenCV };

// 竞争策略：对相同输入在首次调用时分别计时，之后复用更快的算法
inline std::tuple<int, int, int, int> DetectScreenDiffCompetitive(
    const std::vector<BYTE>& prevPixels,
    const std::vector<BYTE>& currPixels,
    int width,
    int height,
    int bytesPerPixel) {
    // 静态局部变量：每个编译单元各自独立，避免全局状态带来的生命周期复杂度
    static bool s_tested = false;
    static Algorithm s_chosen = Algorithm::Custom;

    if (!s_tested) {
        // 自定义算法计时
        auto start = std::chrono::high_resolution_clock::now();
        volatile auto r1 = DetectScreenDiff(prevPixels, currPixels, width, height, bytesPerPixel);
        auto end = std::chrono::high_resolution_clock::now();
        auto customTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

#ifdef USE_OPENCV
        // OpenCV 算法计时
        start = std::chrono::high_resolution_clock::now();
        volatile auto r2 = DetectScreenDiffOpenCVRaw(prevPixels, currPixels, width, height);
        end = std::chrono::high_resolution_clock::now();
        auto opencvTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        s_chosen = (opencvTime < customTime) ? Algorithm::OpenCV : Algorithm::Custom;
#else
        s_chosen = Algorithm::Custom;
#endif
        s_tested = true;
    }

    // 使用选择的算法
    if (s_chosen == Algorithm::Custom) {
        return DetectScreenDiff(prevPixels, currPixels, width, height, bytesPerPixel);
    }
#ifdef USE_OPENCV
    return DetectScreenDiffOpenCVRaw(prevPixels, currPixels, width, height);
#else
    return DetectScreenDiff(prevPixels, currPixels, width, height, bytesPerPixel);
#endif
}

// 辅助函数：创建屏幕数据包
inline std::vector<BYTE> CreateScreenData(const CImage& ScreenImage, int minX, int minY, int maxX, int maxY, int nWidth, int nHeight, int nBitperPixel) {
    std::vector<BYTE> diffData;
    // 诊断输出：打印传入 ScreenImage 的属性，帮助排查传入图像与 width/height 参数不一致的问题
    try {
        int siW = ScreenImage.GetWidth();
        int siH = ScreenImage.GetHeight();
        int siBpp = ScreenImage.GetBPP();
        const void* siBits = ScreenImage.GetBits();
        std::cout << "CreateScreenData: Entered with ScreenImage w=" << siW << " h=" << siH << " bpp=" << siBpp
                  << " bitsPtr=" << siBits << " requested nWidth=" << nWidth << " nHeight=" << nHeight << std::endl;
    } catch (...) {
        std::cout << "CreateScreenData: failed to query ScreenImage properties" << std::endl;
    }
    // 如果 ROI 无效（min>max 或 minY>maxY），则表示无变化：序列化为 (minX,minY,w=0,h=0) 并返回（不包含图像数据）
    if (minX > maxX || minY > maxY) {
        std::cout << "CreateScreenData: No changes detected, serializing empty diff header (" << minX << "," << minY << ")\n";
        int zeroW = 0;
        int zeroH = 0;
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&minX), reinterpret_cast<BYTE*>(&minX) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&minY), reinterpret_cast<BYTE*>(&minY) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&zeroW), reinterpret_cast<BYTE*>(&zeroW) + sizeof(int));
        diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&zeroH), reinterpret_cast<BYTE*>(&zeroH) + sizeof(int));
        // 为了兼容上层协议的最小消息大小（测试期望 header 之后有数据），添加 1 字节占位符
        const BYTE placeholder = 0;
        diffData.push_back(placeholder);
        return diffData;
    }

    if (minX <= maxX && minY <= maxY) {
        std::cout << "Sending diff image: (" << minX << "," << minY << ") to (" << maxX << "," << maxY << ")" << std::endl;
        // 有变化，发送差异
        int diffWidth = maxX - minX + 1;
        int diffHeight = maxY - minY + 1;

        // 防御性检查：避免异常的 ROI 导致溢出或超大分配
        if (diffWidth <= 0 || diffHeight <= 0) {
            throw std::runtime_error("CreateScreenData: computed invalid diff dimensions.");
        }
        if (diffWidth > nWidth || diffHeight > nHeight) {
            std::ostringstream oss;
            oss << "CreateScreenData: clamping diff dims from (" << diffWidth << "," << diffHeight << ") to max (" << nWidth << "," << nHeight << ")";
            std::cout << oss.str() << std::endl;
            diffWidth = std::min(diffWidth, nWidth);
            diffHeight = std::min(diffHeight, nHeight);
        }

        // 创建差异图像
        CImage diffImage;
        // 尝试使用更稳妥的像素格式创建（优先 32bpp，其次 24bpp）
        HRESULT hr = diffImage.Create(diffWidth, diffHeight, 32);
        if (FAILED(hr)) {
            hr = diffImage.Create(diffWidth, diffHeight, 24);
        }
        if (FAILED(hr)) {
            DWORD lastError = GetLastError();
            std::ostringstream oss;
            oss << "Failed to create diff image. hr=0x" << std::hex << hr
                << " lastError=" << std::dec << lastError
                << " w=" << diffWidth << " h=" << diffHeight
                << " requestedBpp=" << nBitperPixel;
            throw std::runtime_error(oss.str());
        }

        // 防御性检查：打印并验证源图像与差异图像内部状态，避免在 CImage 内部触发断言
        try {
            int sW = ScreenImage.GetWidth();
            int sH = ScreenImage.GetHeight();
            int sBpp = ScreenImage.GetBPP();
            const void* sBits = ScreenImage.GetBits();
            std::cout << "CreateScreenData: Source image w=" << sW << " h=" << sH << " bpp=" << sBpp
                      << " bitsPtr=" << sBits << std::endl;
            if (!sBits) {
                throw std::runtime_error("CreateScreenData: source ScreenImage.GetBits() returned nullptr.");
            }
        } catch (const std::exception& ex) {
            std::ostringstream oss;
            oss << "CreateScreenData: failed pre-check on source image: " << ex.what();
            throw std::runtime_error(oss.str());
        }

        // 使用 RAII 包装获取与释放 DC，确保在异常时也能成对释放
        CImageDCRAII srcWrapper(ScreenImage);
        if (!srcWrapper) {
            std::ostringstream oss;
            oss << "CreateScreenData: ScreenImage.GetDC() returned NULL for ROI (" << minX << "," << minY << ")-(" << maxX << "," << maxY << ")";
            throw std::runtime_error(oss.str());
        }

        // 在请求目标 DC 之前再次检查 diffImage 内部状态（保持原有防御性检查）
        try {
            int dW = diffImage.GetWidth();
            int dH = diffImage.GetHeight();
            int dBpp = diffImage.GetBPP();
            const void* dBits = diffImage.GetBits();
            std::cout << "CreateScreenData: Diff image w=" << dW << " h=" << dH << " bpp=" << dBpp
                      << " bitsPtr=" << dBits << std::endl;
            if (!dBits) {
                throw std::runtime_error("CreateScreenData: diffImage.GetBits() returned nullptr.");
            }
        } catch (const std::exception& ex) {
            std::ostringstream oss;
            oss << "CreateScreenData: failed pre-check on diff image: " << ex.what();
            throw std::runtime_error(oss.str());
        }

        CImageDCRAII dstWrapper(diffImage);
        if (!dstWrapper) {
            throw std::runtime_error("Failed to get destination DC for diff.");
        }

        if (::BitBlt(dstWrapper.get(), 0, 0, diffWidth, diffHeight, srcWrapper.get(), minX, minY, SRCCOPY) == FALSE) {
            throw std::runtime_error("BitBlt for diff failed.");
        }

        // 保存差异图像为 PNG
        IStream* pStreamRaw = nullptr;
        hr = CreateStreamOnHGlobal(nullptr, TRUE, &pStreamRaw);
        if (FAILED(hr)) {
            throw std::runtime_error("CreateStreamOnHGlobal failed for diff.");
        }
        ScopedStream pStream(pStreamRaw);

        try {
            SaveCImageToStreamFallback(diffImage, pStream.get());
        } catch (const std::exception& ex) {
            std::ostringstream oss;
            oss << "Failed to save diff image: " << ex.what();
            throw std::runtime_error(oss.str());
        }

        HGLOBAL hGlobal = nullptr;
        hr = GetHGlobalFromStream(pStream.get(), &hGlobal);
        if (FAILED(hr) || hGlobal == nullptr) {
            throw std::runtime_error("GetHGlobalFromStream failed for diff.");
        }

        std::cout << "CreateScreenData: GetHGlobalFromStream returned hGlobal=" << hGlobal << std::endl;

        GlobalLockRAII lock(hGlobal);
        BYTE* pData = static_cast<BYTE*>(lock.get());
        if (pData == nullptr) {
            throw std::runtime_error("GlobalLock failed for diff.");
        }

        std::cout << "CreateScreenData: GlobalLock succeeded, pData=" << static_cast<void*>(pData) << std::endl;
        size_t dataSize = ::GlobalSize(hGlobal);
        if (dataSize == 0) {
            throw std::runtime_error("GlobalSize returned zero for diff.");
        }
        std::cout << "CreateScreenData: diff PNG dataSize=" << dataSize << " bytes" << std::endl;

        // debug-saving of diff PNG removed to silence debug logs and disk writes
        // 防御性上限，防止某些异常情况返回极大 size 导致内存/IO 崩溃
        const size_t MAX_PNG_SIZE = 50ull * 1024ull * 1024ull; // 50 MB
        if (dataSize > MAX_PNG_SIZE) {
            std::ostringstream oss;
            oss << "CreateScreenData: PNG dataSize too large (" << dataSize << " bytes) - aborting to avoid OOM.";
            std::cerr << oss.str() << std::endl;
            throw std::runtime_error("CreateScreenData: PNG payload exceeds allowed maximum size.");
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

        try {
            SaveCImageToStreamFallback(ScreenImage, pStream.get());
        } catch (const std::exception& ex) {
            int sW = ScreenImage.GetWidth();
            int sH = ScreenImage.GetHeight();
            int sBpp = ScreenImage.GetBPP();
            const void* sBits = ScreenImage.GetBits();
            std::ostringstream oss;
            oss << "Failed to save image to stream: " << ex.what()
                << " src(w=" << sW << " h=" << sH << " bpp=" << sBpp << " bitsPtr=" << sBits << ")";
            throw std::runtime_error(oss.str());
        }

        HGLOBAL hGlobal = nullptr;
        hr = GetHGlobalFromStream(pStream.get(), &hGlobal);
        if (FAILED(hr) || hGlobal == nullptr) {
            throw std::runtime_error("GetHGlobalFromStream failed.");
        }

        std::cout << "CreateScreenData: GetHGlobalFromStream (full) returned hGlobal=" << hGlobal << std::endl;

        GlobalLockRAII lock(hGlobal);
        BYTE* pData = static_cast<BYTE*>(lock.get());
        if (pData == nullptr) {
            throw std::runtime_error("GlobalLock failed.");
        }

        std::cout << "CreateScreenData: GlobalLock succeeded (full), pData=" << static_cast<void*>(pData) << std::endl;
        size_t dataSize = ::GlobalSize(hGlobal);
        if (dataSize == 0) {
            throw std::runtime_error("GlobalSize returned zero.");
        }
        std::cout << "CreateScreenData: full PNG dataSize=" << dataSize << " bytes" << std::endl;

        // debug-saving of full PNG removed to silence debug logs and disk writes
        const size_t MAX_PNG_SIZE_FULL = 100ull * 1024ull * 1024ull; // 100 MB for full-screen
        if (dataSize > MAX_PNG_SIZE_FULL) {
            std::ostringstream oss;
            oss << "CreateScreenData: full-screen PNG too large (" << dataSize << " bytes) - aborting to avoid OOM.";
            std::cerr << oss.str() << std::endl;
            throw std::runtime_error("CreateScreenData: full-screen PNG payload exceeds allowed maximum size.");
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
    // 增加健壮性检查：如果传入的 CImage 对象无效，则直接返回空数据包，防止崩溃
    if (screenImage.IsNull()) {
        std::cerr << "Error: GenerateScreenData received a null CImage object." << std::endl;
        return {}; // 返回一个空 vector
    }

    std::cout << "[GenerateScreenData] ENTRY: width=" << width << " height=" << height
              << " prevBytes=" << prevPixels.size() << " currBytes=" << currPixels.size()
              << " bpp=" << bitPerPixel << std::endl;
    // 基本健壮性校验：宽高与像素缓冲
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("GenerateScreenData: invalid width/height");
    }
    
    int bytesPerPixel = bitPerPixel / 8;
    if (bytesPerPixel <= 0) bytesPerPixel = 4; // Fallback
    const size_t expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height) * bytesPerPixel;
    
    // 如果当前帧像素大小异常，按首次帧处理
    if (currPixels.size() < expectedSize) {
        std::cout << "[GenerateScreenData] currPixels size mismatch: got " << currPixels.size() 
                  << " expected " << expectedSize << " - treating as full screen" << std::endl;
        return CreateScreenData(screenImage, 0, 0, width - 1, height - 1, width, height, bitPerPixel);
    }

    std::cout << "[GenerateScreenData] Calling DetectScreenDiff..." << std::endl;

    // 使用兼顾 stride 的差异检测（会考虑 bytesPerPixel），以保持
    // "差异比对 + 位置粘贴" 的合成视频效果
    auto [minX, minY, maxX, maxY] = DetectScreenDiffCompetitive(prevPixels, currPixels, width, height, bytesPerPixel);

    std::cout << "[GenerateScreenData] DetectScreenDiff returned: minX=" << minX << " minY=" << minY 
              << " maxX=" << maxX << " maxY=" << maxY << std::endl;

    // 边界收缩，避免 ROI 越界
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= width) maxX = width - 1;
    if (maxY >= height) maxY = height - 1;

    // 若收缩后无效，按无变化处理（将触发全屏头部分支）
    if (minX > maxX || minY > maxY) {
        std::cout << "[GenerateScreenData] After bounds checking: invalid ROI detected, treating as no changes" << std::endl;
        minX = 1; minY = 1; maxX = 0; maxY = 0; // 无变化矩形
    }

    // 记录调试信息
    std::cout << "[GenerateScreenData] Final ROI: (" << minX << "," << minY << ")-(" << maxX << "," << maxY << ")"
              << " size=" << width << "x" << height << " currBytes=" << currPixels.size() << std::endl;
    return CreateScreenData(screenImage, minX, minY, maxX, maxY, width, height, bitPerPixel);
}