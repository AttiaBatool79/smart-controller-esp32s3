#include "status.hpp"

#include <cinttypes>
#include <cstdio>

#include "esp_system.h"
#include "esp_timer.h"
#include "io.hpp"
#include "mqtt_link.hpp"
#include "ota.hpp"
#include "schedule.hpp"
#include "time_sync.hpp"
#include "wifi.hpp"

namespace status {

std::string Json() {
    char buf[640];
    std::snprintf(
        buf, sizeof(buf),
        "{\"id\":\"%s\",\"version\":\"%s\",\"partition\":\"%s\","
        "\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"ap_mode\":%s,\"ap_ssid\":\"%s\","
        "\"mqtt\":%s,\"time\":\"%s\",\"uptime_s\":%" PRId64 ",\"free_heap\":%" PRIu32
        ",\"chip_temp_c\":%.1f,\"relays\":[%s,%s],\"inputs\":[%s,%s],\"schedule\":",
        net::DeviceId().c_str(), ota::Version(), ota::RunningPartition(),
        net::WifiSsid().c_str(), net::WifiIp().c_str(), net::WifiRssi(),
        net::ApActive() ? "true" : "false", net::ApSsid().c_str(),
        mqtt::IsConnected() ? "true" : "false", clk::TimeNow().c_str(),
        esp_timer_get_time() / 1000000, esp_get_free_heap_size(), io::ChipTempC(),
        io::RelayGet(0) ? "true" : "false", io::RelayGet(1) ? "true" : "false",
        io::InputActive(0) ? "true" : "false", io::InputActive(1) ? "true" : "false");
    return std::string(buf) + schedule::ToJson() + "}";
}

}  // namespace status
