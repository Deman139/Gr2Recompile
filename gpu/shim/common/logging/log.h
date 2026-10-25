// gr2: replaces shadPS4's spdlog-based logger with a small fmt sink.
// Level from GR2_GPU_LOG (trace|debug|info|warning|error; default warning).
#pragma once
#include <algorithm>
#include <atomic>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <string_view>
#include <fmt/format.h>
#include "common/logging/classes.h"
#include "common/path_util.h"
#include "common/thread.h"

// Level type keeps shadPS4's spdlog spelling so vendored callers compile unchanged.
namespace spdlog {
enum class level : int { trace, debug, info, warn, err, critical, off };
}
namespace Common::Log {
using Level = spdlog::level;
bool ShouldLog(Level level);
void Write(std::string_view log_class, Level level, const char* file, int line, const char* function,
           std::string&& message);
/// gr2: a call site logs its first messages only (GR2_GPU_LOG_REPEATS, default 20; 0 = all).
/// Per-draw warnings otherwise flood the unbuffered stderr from the GPU command thread: in
/// Killzone stream-out scenes the writes took most of that thread's time (6 FPS).
bool AllowSite(std::atomic<unsigned>& count, const char* file, int line);
inline void Flush() {}
} // namespace Common::Log

#define LOG_GENERIC(log_class, log_level, fmt_str, ...)                                             \
    do {                                                                                           \
        static std::atomic<unsigned> gr2_log_site_count{0};                                      \
        if (Common::Log::ShouldLog(log_level) &&                                                   \
            Common::Log::AllowSite(gr2_log_site_count, __FILE__, __LINE__)) {                     \
            Common::Log::Write(log_class, log_level, __FILE__, __LINE__, __func__,                 \
                               fmt::format(fmt_str __VA_OPT__(, ) __VA_ARGS__));                    \
        }                                                                                          \
    } while (false)
#define LOG_TRACE(log_class, ...) LOG_GENERIC(Common::Log::Class::log_class, spdlog::level::trace, __VA_ARGS__)
#define LOG_DEBUG(log_class, ...) LOG_GENERIC(Common::Log::Class::log_class, spdlog::level::debug, __VA_ARGS__)
#define LOG_INFO(log_class, ...) LOG_GENERIC(Common::Log::Class::log_class, spdlog::level::info, __VA_ARGS__)
#define LOG_WARNING(log_class, ...) LOG_GENERIC(Common::Log::Class::log_class, spdlog::level::warn, __VA_ARGS__)
#define LOG_ERROR(log_class, ...) LOG_GENERIC(Common::Log::Class::log_class, spdlog::level::err, __VA_ARGS__)
#define LOG_CRITICAL(log_class, ...) LOG_GENERIC(Common::Log::Class::log_class, spdlog::level::critical, __VA_ARGS__)
