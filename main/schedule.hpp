#pragma once
// Daily relay timers, e.g. Relay 1 ON at 18:00, OFF at 23:00.
// Saved in NVS, so they survive reboot. Needs a valid clock (NTP).

#include <string>

namespace schedule {

struct Rule {
    bool enabled;
    int on_minute;   // minutes after midnight, 18:00 -> 1080
    int off_minute;
};

void Start();  // load from NVS and start the checker task
bool Set(int relay_index, const Rule& rule);
Rule Get(int relay_index);
std::string ToJson();  // [{"enabled":true,"on":"18:00","off":"23:00"},...]
//! "18:30" -> 1110, -1 if invalid
int ParseHHMM(const char* text);

}  // namespace schedule
