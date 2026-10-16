// gow3: see log.h.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include "common/logging/log.h"

namespace Common::Log {
static Level threshold() {
    static const Level level = [] {
        const char* v = std::getenv("GOW3_GPU_LOG");
        if (!v) return Level::warn;
        static const char* names[] = {"trace", "debug", "info", "warning", "error", "critical"};
        for (int i = 0; i < 6; ++i)
            if (!std::strcmp(v, names[i])) return Level(i);
        return Level::warn;
    }();
    return level;
}
bool ShouldLog(Level level) { return level >= threshold(); }
bool AllowSite(std::atomic<unsigned>& count, const char* file, int line) {
    static const unsigned limit = [] {
        const char* v = std::getenv("GOW3_GPU_LOG_REPEATS");
        return v ? unsigned(std::strtoul(v, nullptr, 10)) : 20u;
    }();
    if (!limit) return true;
    const unsigned n = count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n == limit + 1) {
        const char* base = std::strrchr(file, '/');
        std::fprintf(stderr, "GPU: %s:%d logged %u times, further messages from it are suppressed\n",
                     base ? base + 1 : file, line, limit);
    }
    return n <= limit;
}
void Write(std::string_view log_class, Level level, const char* file, int line, const char* function,
           std::string&& message) {
    static std::mutex mutex;
    static const char* names[] = {"Trace", "Debug", "Info", "Warning", "Error", "Critical"};
    const char* base = std::strrchr(file, '/');
    std::scoped_lock lock{mutex};
    std::fprintf(stderr, "GPU [%.*s] <%s> %s:%d %s: %s\n", int(log_class.size()), log_class.data(),
                 names[int(level)], base ? base + 1 : file, line, function, message.c_str());
}
} // namespace Common::Log
