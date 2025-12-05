#pragma once

#include <chrono>
#include <cstdint>

using std::uint64_t;

class TimeUtils {
public:
    static uint64_t getCurrentTimestampMs();
    static bool hasTimeout(uint64_t startTimeMs, int timeoutMs);
};