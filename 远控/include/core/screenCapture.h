#pragma once

#include <vector>
#include <memory>
#include <stdexcept>
#include <comutil.h>
#include <iostream>

#include "pch.h"
#include "Enities.h"
#include "ServerSocket.h"
#include "diffAlgorithm.h"

using namespace Gdiplus;

// 辅助函数：捕获屏幕图像并返回图像和像素数据
inline std::tuple<CImage, std::vector<BYTE>, int, int, int> CaptureScreenImage() {
    ScopedHDC hScreenDC(::GetDC(nullptr));
    if (!hScreenDC) {
        throw std::runtime_error("Failed to get screen DC.");
    }
    
    int nWidth = ::GetDeviceCaps(hScreenDC.get(), HORZRES);
    int nHeight = ::GetDeviceCaps(hScreenDC.get(), VERTRES);
    int nBitperPixel = ::GetDeviceCaps(hScreenDC.get(), BITSPIXEL);

    if(nWidth == 0 || nHeight == 0 || nBitperPixel == 0) {
        throw std::runtime_error("Invalid screen dimensions or bit depth.");
    }

    CImage ScreenImage;
    if (ScreenImage.Create(nWidth, nHeight, nBitperPixel) != S_OK) {
        throw std::runtime_error("Failed to create CImage.");
    }

    HDC hImageDC = ScreenImage.GetDC();
    if (!hImageDC) {
        throw std::runtime_error("Failed to get image DC.");
    }

    if(::BitBlt(hImageDC, 0, 0, nWidth, nHeight, hScreenDC.get(), 0, 0, SRCCOPY) == FALSE) {
        ScreenImage.ReleaseDC();
        throw std::runtime_error("BitBlt failed.");
    }
    ScreenImage.ReleaseDC();

    // 获取当前帧像素数据
    int stride = nWidth * 4; // 假设 32-bit
    BYTE* currentPixels = reinterpret_cast<BYTE*>(ScreenImage.GetBits());
    std::vector<BYTE> currentFramePixels(currentPixels, currentPixels + nHeight * stride);

    return {std::move(ScreenImage), currentFramePixels, nWidth, nHeight, nBitperPixel};
}

inline void CaptureScreen(CServerSocket& ClientSocket, const Cpacket& packet) {
    static std::vector<BYTE> previousFramePixels; // 存储上一帧的像素数据
    static int prevWidth = 0, prevHeight = 0;

    try {
        auto [ScreenImage, currentFramePixels, nWidth, nHeight, nBitperPixel] = CaptureScreenImage();

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