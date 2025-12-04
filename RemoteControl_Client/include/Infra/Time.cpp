#include "pch.h"
#include "Time.hpp"

uint64_t TimeUtils::getCurrentTimestampMs() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

bool TimeUtils::hasTimeout(uint64_t startTimeMs, int timeoutMs) {
    uint64_t current = getCurrentTimestampMs();
    return (current - startTimeMs) > static_cast<uint64_t>(timeoutMs);
}