#pragma once

#include "Protocol/Infra/LoggerProtocol.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <thread>
#include <chrono>
#include <iomanip>

class Logger : public LoggerBase {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void log(Level level, const std::string& message) {
        if (level < currentLevel) return;

        std::ostringstream logStream;
        logStream << getCurrentTime() << " [" << levelToString(level) << "] "
                  << "[Thread " << std::this_thread::get_id() << "] "
                  << message;

        std::lock_guard<std::mutex> lock(mutex);
        if (outputTarget == FILE || outputTarget == BOTH) {
            std::ofstream ofs(logFile, std::ios::app);
            if (ofs) {
                ofs << logStream.str() << std::endl;
            }
        }
        if (outputTarget == CONSOLE || outputTarget == BOTH) {
            std::cout << logStream.str() << std::endl;
        }
    }

    void setLogLevel(Level level) {
        std::lock_guard<std::mutex> lock(mutex);
        currentLevel = level;
    }

    void setOutputTarget(OutputTarget target) {
        std::lock_guard<std::mutex> lock(mutex);
        outputTarget = target;
    }

    void setLogFile(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(mutex);
        logFile = filePath;
    }

private:
    Logger() = default;
    ~Logger() = default;

    std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::ostringstream timeStream;
        timeStream << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S")
                   << "." << std::setfill('0') << std::setw(3) << ms.count();
        return timeStream.str();
    }
};