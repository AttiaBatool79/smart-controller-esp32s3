#include "schedule.hpp"

#include <cstdio>
#include <ctime>
#include <mutex>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "io.hpp"
#include "storage.hpp"
#include "time_sync.hpp"

namespace schedule {
namespace {

struct Stored {  // what goes into NVS (fixed layout)
    uint8_t enabled;
    uint16_t on_minute;
    uint16_t off_minute;
};

Rule s_rules[io::kRelayCount] = {};
std::mutex s_mutex;

void Key(int i, char* key, size_t size) { std::snprintf(key, size, "sched%d", i + 1); }

std::string HHMM(int minute) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", (minute / 60) % 24, minute % 60);
    return buf;
}

// Once per new minute: switch relays whose ON/OFF time is now. Only the
// exact minute triggers, so a manual change in between is respected.
void Task(void*) {
    int last_minute = -1;
    while (true) {
        if (clk::TimeIsValid()) {
            time_t now = time(nullptr);
            tm t{};
            localtime_r(&now, &t);
            const int minute = t.tm_hour * 60 + t.tm_min;
            if (minute != last_minute) {
                last_minute = minute;
                for (int i = 0; i < io::kRelayCount; ++i) {
                    const Rule r = Get(i);
                    if (!r.enabled) continue;
                    if (minute == r.on_minute) io::RelaySet(i, true, "schedule");
                    if (minute == r.off_minute) io::RelaySet(i, false, "schedule");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

}  // namespace

int ParseHHMM(const char* text) {
    int h = -1;
    int m = -1;
    if (std::sscanf(text, "%d:%d", &h, &m) != 2) return -1;
    if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
    return h * 60 + m;
}

void Start() {
    for (int i = 0; i < io::kRelayCount; ++i) {
        char key[12];
        Key(i, key, sizeof(key));
        Stored st{};
        if (storage::GetBlob(key, &st, sizeof(st))) {
            s_rules[i] = {st.enabled != 0, st.on_minute, st.off_minute};
        } else {
            s_rules[i] = {false, 18 * 60, 23 * 60};  // default 18:00-23:00, off
        }
    }
    xTaskCreate(Task, "schedule", 3072, nullptr, 4, nullptr);
    ESP_LOGI("schedule", "R1 %s %s-%s | R2 %s %s-%s",
             s_rules[0].enabled ? "ON" : "off", HHMM(s_rules[0].on_minute).c_str(),
             HHMM(s_rules[0].off_minute).c_str(), s_rules[1].enabled ? "ON" : "off",
             HHMM(s_rules[1].on_minute).c_str(), HHMM(s_rules[1].off_minute).c_str());
}

bool Set(int i, const Rule& rule) {
    if (i < 0 || i >= io::kRelayCount) return false;
    if (rule.on_minute < 0 || rule.on_minute >= 1440 || rule.off_minute < 0 ||
        rule.off_minute >= 1440) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_rules[i] = rule;
    }
    char key[12];
    Key(i, key, sizeof(key));
    Stored st{static_cast<uint8_t>(rule.enabled ? 1 : 0),
              static_cast<uint16_t>(rule.on_minute),
              static_cast<uint16_t>(rule.off_minute)};
    storage::SetBlob(key, &st, sizeof(st));
    return true;
}

Rule Get(int i) {
    std::lock_guard<std::mutex> lock(s_mutex);
    return (i >= 0 && i < io::kRelayCount) ? s_rules[i] : Rule{};
}

std::string ToJson() {
    std::string out = "[";
    for (int i = 0; i < io::kRelayCount; ++i) {
        const Rule r = Get(i);
        if (i) out += ',';
        out += std::string("{\"enabled\":") + (r.enabled ? "true" : "false") +
               ",\"on\":\"" + HHMM(r.on_minute) + "\",\"off\":\"" +
               HHMM(r.off_minute) + "\"}";
    }
    return out + "]";
}

}  // namespace schedule
