// Smart Controller - ESP32-S3 (KinCony KC868-A2v3)
// Web dashboard, 2 relays, daily timers, local inputs, MQTT, OTA updates.

#include <cinttypes>
#include <cstdio>

#include "config.hpp"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "events.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "io.hpp"
#include "mqtt_link.hpp"
#include "oled.hpp"
#include "ota.hpp"
#include "schedule.hpp"
#include "storage.hpp"
#include "time_sync.hpp"
#include "web.hpp"
#include "wifi.hpp"

static const char* TAG = "smart";

static void PrintBoardInfo() {
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    uint32_t flash_size = 0;
    esp_flash_get_size(nullptr, &flash_size);
    ESP_LOGI(TAG, "=== Smart Controller %s (slot %s) ===", ota::Version(),
             ota::RunningPartition());
    ESP_LOGI(TAG, "ESP32-S3, %d cores, rev v%d.%d, flash %" PRIu32 " MB, id %s",
             chip.cores, chip.revision / 100, chip.revision % 100,
             flash_size / (1024 * 1024), net::DeviceId().c_str());
}

// OLED: 5 lines, refreshed every second.
static void DisplayTask(void*) {
    char line[24];
    while (true) {
        oled::Clear();
        if (net::ApActive() && !net::WifiIsConnected()) {
            oled::Text(0, 0, "SETUP MODE");
            std::snprintf(line, sizeof(line), "WiFi %s", net::ApSsid().c_str());
            oled::Text(0, 13, line);
            std::snprintf(line, sizeof(line), "Pass %s", cfg::kApPassword);
            oled::Text(0, 26, line);
            oled::Text(0, 39, "Open 192.168.4.1");
        } else {
            oled::Text(0, 0, net::WifiIsConnected() ? "Smart Controller"
                                                    : "WiFi connecting...");
            std::snprintf(line, sizeof(line), "IP %s", net::WifiIp().c_str());
            oled::Text(0, 13, line);
            std::snprintf(line, sizeof(line), "R1:%s R2:%s %s",
                          io::RelayGet(0) ? "ON" : "OFF",
                          io::RelayGet(1) ? "ON" : "OFF",
                          mqtt::IsConnected() ? "MQTT" : "");
            oled::Text(0, 26, line);
            oled::Text(0, 39, clk::Format("%d-%m-%Y").c_str());
        }
        oled::Text(0, 52, clk::Format("%H:%M:%S").c_str());
        oled::Show();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main(void) {
    storage::Init();  // NVS first: everything else reads settings from it
    PrintBoardInfo();
    events::Add("Boot, firmware %s", ota::Version());

    io::Init();  // relays come back in their last state
    if (oled::Init(cfg::kI2cSda, cfg::kI2cScl)) {
        oled::Text(0, 0, "Smart Controller");
        oled::Text(0, 13, "Booting...");
        oled::Show();
        xTaskCreate(DisplayTask, "display", 4096, nullptr, 3, nullptr);
    }

    net::WifiStart();
    clk::TimeSyncStart();
    web::Start();
    schedule::Start();
    mqtt::Start();

    bool healthy = false;
    while (true) {
        // New firmware counts as good once it has run 30 s with WiFi up.
        if (!healthy && net::WifiIsConnected() &&
            esp_timer_get_time() > 30LL * 1000 * 1000) {
            ota::MarkHealthy();
            healthy = true;
        }
        ESP_LOGI(TAG, "IP %s | R1 %s R2 %s | %s | heap %" PRIu32,
                 net::WifiIp().c_str(), io::RelayGet(0) ? "ON" : "OFF",
                 io::RelayGet(1) ? "ON" : "OFF", clk::TimeNow().c_str(),
                 esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
