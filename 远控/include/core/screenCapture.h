#pragma once

#include <vector>
#include <memory>
#include <stdexcept>
#include <comutil.h>

#include "pch.h"
#include "Enities.h"
#include "ServerSocket.h"
#include "diffAlgorithm.h"

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



inline void CaptureScreen(CServerSocket& ClientSocket, const Cpacket& packet) {
    static std::vector<BYTE> previousFramePixels; // 存储上一帧的像素数据
    static int prevWidth = 0, prevHeight = 0;

    try {
        ScopedHDC hScreenDC(::GetDC(nullptr));
        if (!hScreenDC) {
            std::string errMsg = "Failed to get screen DC.";
            ClientSocket.SendErrorPacket(errMsg);
            return;
        }
        
        int nWidth = ::GetDeviceCaps(hScreenDC.get(), HORZRES);
        int nHeight = ::GetDeviceCaps(hScreenDC.get(), VERTRES);
        int nBitperPixel = ::GetDeviceCaps(hScreenDC.get(), BITSPIXEL);

        if(nWidth == 0 || nHeight == 0 || nBitperPixel == 0) {
            std::string errMsg = "Invalid screen dimensions or bit depth.";
            ClientSocket.SendErrorPacket(errMsg);
            return;
        }

        CImage ScreenImage;
        if (ScreenImage.Create(nWidth, nHeight, nBitperPixel) != S_OK) {
            std::string errMsg = "Failed to create CImage.";
            ClientSocket.SendErrorPacket(errMsg);
            return;
        }

        HDC hImageDC = ScreenImage.GetDC();
        if (!hImageDC) {
            std::string errMsg = "Failed to get image DC.";
            ClientSocket.SendErrorPacket(errMsg);  
            return;
        }

        if(::BitBlt(hImageDC, 0, 0, nWidth, nHeight, hScreenDC.get(), 0, 0, SRCCOPY) == FALSE) {
            ScreenImage.ReleaseDC();
            std::string errMsg = "BitBlt failed.";
            ClientSocket.SendErrorPacket(errMsg);  
            return;
        }
        ScreenImage.ReleaseDC();

    // 获取当前帧像素数据
        int stride = nWidth * 4; // 假设 32-bit
        BYTE* currentPixels = reinterpret_cast<BYTE*>(ScreenImage.GetBits());
        std::vector<BYTE> currentFramePixels(currentPixels, currentPixels + nHeight * stride);

    // 检测差异，使用竞争策略（内部可选 OpenCV，无需在此处耦合）
    auto [minX, minY, maxX, maxY] = DetectScreenDiffCompetitive(previousFramePixels, currentFramePixels, nWidth, nHeight);

        if (minX <= maxX && minY <= maxY) {
            // 有变化，发送差异
            int diffWidth = maxX - minX + 1;
            int diffHeight = maxY - minY + 1;

            // 创建差异图像
            CImage diffImage;
            if (diffImage.Create(diffWidth, diffHeight, nBitperPixel) != S_OK) {
                std::string errMsg = "Failed to create diff image.";
                ClientSocket.SendErrorPacket(errMsg);
                return;
            }

            HDC srcDC = ScreenImage.GetDC();
            HDC dstDC = diffImage.GetDC();
            if (::BitBlt(dstDC, 0, 0, diffWidth, diffHeight, srcDC, minX, minY, SRCCOPY) == FALSE) {
                ScreenImage.ReleaseDC();
                diffImage.ReleaseDC();
                std::string errMsg = "BitBlt for diff failed.";
                ClientSocket.SendErrorPacket(errMsg);
                return;
            }
            ScreenImage.ReleaseDC();
            diffImage.ReleaseDC();

            // 保存差异图像为 PNG
            IStream* pStreamRaw = nullptr;
            HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &pStreamRaw);
            if (FAILED(hr)) {
                std::string errMsg = "CreateStreamOnHGlobal failed for diff.";
                ClientSocket.SendErrorPacket(errMsg);
                return;
            }
            ScopedStream pStream(pStreamRaw);

            hr = diffImage.Save(pStream.get(), Gdiplus::ImageFormatPNG);
            if (FAILED(hr)) {
                std::string errMsg = "Failed to save diff image.";
                ClientSocket.SendErrorPacket(errMsg);
                return;
            }

            HGLOBAL hGlobal = nullptr;
            hr = GetHGlobalFromStream(pStream.get(), &hGlobal);
            if (FAILED(hr) || hGlobal == nullptr) {
                std::string errMsg = "GetHGlobalFromStream failed for diff.";
                ClientSocket.SendErrorPacket(errMsg);
                return;
            }

            GlobalLockRAII lock(hGlobal);
            BYTE* pData = static_cast<BYTE*>(lock.get());
            if (pData == nullptr) {
                std::string errMsg = "GlobalLock failed for diff.";
                ClientSocket.SendErrorPacket(errMsg);
                return;
            }

            size_t dataSize = ::GlobalSize(hGlobal);
            if (dataSize == 0) {
                std::string errMsg = "GlobalSize returned zero for diff.";
                ClientSocket.SendErrorPacket(errMsg);
                return;
            }

            // 序列化数据：x, y, w, h + PNG
            std::vector<BYTE> diffData;
            diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&minX), reinterpret_cast<BYTE*>(&minX) + sizeof(int));
            diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&minY), reinterpret_cast<BYTE*>(&minY) + sizeof(int));
            diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&diffWidth), reinterpret_cast<BYTE*>(&diffWidth) + sizeof(int));
            diffData.insert(diffData.end(), reinterpret_cast<BYTE*>(&diffHeight), reinterpret_cast<BYTE*>(&diffHeight) + sizeof(int));
            diffData.insert(diffData.end(), pData, pData + dataSize);

            Cpacket diffPacket(CMD::CMD_SCREEN_DIFF, diffData);
            ClientSocket.SendPacket(diffPacket);
        } else {
            // 无变化或首次，发送全屏
            IStream* pStreamRaw = nullptr;
            HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &pStreamRaw);
            if (FAILED(hr)) {
                std::string errMsg = "CreateStreamOnHGlobal failed.";
                ClientSocket.SendErrorPacket(errMsg);
                return;
            }
            ScopedStream pStream(pStreamRaw);

            hr = ScreenImage.Save(pStream.get(), Gdiplus::ImageFormatPNG);
            if (FAILED(hr)) {
                std::string errMsg = "Failed to save image to stream.";
                ClientSocket.SendErrorPacket(errMsg);
                return;
            }

            HGLOBAL hGlobal = nullptr;
            hr = GetHGlobalFromStream(pStream.get(), &hGlobal);
            if (FAILED(hr) || hGlobal == nullptr) {
                std::string errMsg = "GetHGlobalFromStream failed.";
                ClientSocket.SendErrorPacket(errMsg);
                return;
            }

            GlobalLockRAII lock(hGlobal);
            BYTE* pData = static_cast<BYTE*>(lock.get());
            if (pData == nullptr) {
                std::string errMsg = "GlobalLock failed.";
                ClientSocket.SendErrorPacket(errMsg);
                return;
            }

            size_t dataSize = ::GlobalSize(hGlobal);
            if (dataSize == 0) {
                std::string errMsg = "GlobalSize returned zero.";
                ClientSocket.SendErrorPacket(errMsg);
                return;
            }

            std::vector<BYTE> data(pData, pData + dataSize);
            Cpacket screenPacket(CMD::CMD_SCREEN_CAPTURE, data);
            ClientSocket.SendPacket(screenPacket);
        }

    // 更新上一帧
    previousFramePixels = currentFramePixels;
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