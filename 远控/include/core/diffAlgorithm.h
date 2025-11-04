#pragma once

#include <vector>
#include <tuple>
#include <cstring>
#include <chrono>

using BYTE = unsigned char;

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