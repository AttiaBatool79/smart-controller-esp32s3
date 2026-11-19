#pragma once
#include <string>

namespace clk {
void TimeSyncStart();      // internet (NTP) se time lena shuru
bool TimeIsValid();        // kya time sahi set ho chuka hai?
std::string TimeNow();     // "2026-11-19 12:05:30"
std::string Format(const char* fmt);  // e.g. "%H:%M:%S"
}  // namespace clk
