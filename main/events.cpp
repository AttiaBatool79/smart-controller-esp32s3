#include "events.hpp"

#include <cstdarg>
#include <cstdio>
#include <mutex>

#include "esp_log.h"
#include "time_sync.hpp"

namespace events {
namespace {

constexpr int kMax = 20;
std::string s_items[kMax];
int s_next = 0;   // where the next event goes
int s_count = 0;
std::mutex s_mutex;

std::string JsonEscape(const std::string& in) {
    std::string out;
    for (char c : in) {
        if (c == '"' || c == '\\') out += '\\';
        if (static_cast<unsigned char>(c) >= 0x20) out += c;
    }
    return out;
}

}  // namespace

void Add(const char* fmt, ...) {
    char msg[96];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    ESP_LOGI("event", "%s", msg);

    std::string stamp = clk::TimeIsValid() ? clk::Format("%H:%M:%S") : "--:--:--";
    std::lock_guard<std::mutex> lock(s_mutex);
    s_items[s_next] = stamp + " " + msg;
    s_next = (s_next + 1) % kMax;
    if (s_count < kMax) ++s_count;
}

std::string ToJsonArray() {
    std::lock_guard<std::mutex> lock(s_mutex);
    std::string out = "[";
    for (int i = 0; i < s_count; ++i) {
        const int idx = (s_next - 1 - i + kMax) % kMax;  // newest first
        if (i) out += ',';
        out += '"' + JsonEscape(s_items[idx]) + '"';
    }
    return out + "]";
}

}  // namespace events
