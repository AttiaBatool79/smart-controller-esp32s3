#include "time_sync.hpp"

#include <cstdlib>
#include <ctime>

#include "esp_log.h"
#include "esp_netif_sntp.h"

namespace clk {

void TimeSyncStart() {
    setenv("TZ", "PKT-5", 1);  // Pakistan time = UTC+5
    tzset();
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&cfg);
    ESP_LOGI("time", "NTP sync started (pool.ntp.org)");
}

bool TimeIsValid() {
    time_t now = time(nullptr);
    tm t{};
    localtime_r(&now, &t);
    return t.tm_year + 1900 >= 2024;
}

std::string Format(const char* fmt) {
    if (!TimeIsValid()) return "--";
    time_t now = time(nullptr);
    tm t{};
    localtime_r(&now, &t);
    char buf[32];
    strftime(buf, sizeof(buf), fmt, &t);
    return buf;
}

std::string TimeNow() {
    return TimeIsValid() ? Format("%Y-%m-%d %H:%M:%S") : "time not set";
}

}  // namespace clk
