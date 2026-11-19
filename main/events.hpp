#pragma once
// In-memory event log (last 20 events) shown on the dashboard,
// e.g. "12:05:10 Relay 1 ON (schedule)".

#include <string>

namespace events {
void Add(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
std::string ToJsonArray();  // ["12:05:10 Relay 1 ON (web)", ...] newest first
}  // namespace events
