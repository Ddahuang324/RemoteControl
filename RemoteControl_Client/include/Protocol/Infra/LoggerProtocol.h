#pragma once

#include <string>
#include <mutex>

// Base structure for logger protocol
struct LoggerBase {
    enum Level { INFO, DEBUG, ERROR };
    enum OutputTarget { FILE, CONSOLE, BOTH };

    std::mutex mutex; // Mutex for thread-safe logging
    Level currentLevel = INFO;
    OutputTarget outputTarget = FILE;
    std::string logFile = "application.log";

    LoggerBase() = default;
    virtual ~LoggerBase() = default;

    static std::string levelToString(Level level) {
        switch (level) {
            case INFO: return "INFO";
            case DEBUG: return "DEBUG";
            case ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};