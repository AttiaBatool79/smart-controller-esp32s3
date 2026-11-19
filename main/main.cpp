#include <cinttypes>
#include <cstdio>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oled.hpp"
#include "secrets.h"
#include "time_sync.hpp"
#include "wifi.hpp"

static const char* TAG = "smart";

// KinCony KC868-A2v3 on-board I2C (OLED + DS3231 RTC)
constexpr int kI2cSda = 48;
constexpr int kI2cScl = 47;

static void PrintBoardInfo() {
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    uint32_t flash_size = 0;
    esp_flash_get_size(nullptr, &flash_size);
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "=== Smart Controller v0.3 ===");
    ESP_LOGI(TAG, "Chip: ESP32-S3, %d cores, rev v%d.%d", chip.cores,
             chip.revision / 100, chip.revision % 100);
    ESP_LOGI(TAG, "Flash: %" PRIu32 " MB", flash_size / (1024 * 1024));
    ESP_LOGI(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
             mac[2], mac[3], mac[4], mac[5]);
}

// Refreshes the OLED once per second. Lines are 12 px apart (5 lines).
static void DisplayTask(void*) {
    char line[24];
    while (true) {
        oled::Clear();
        oled::Text(0, 0, "Smart Controller");

        if (net::WifiIsConnected()) {
            std::snprintf(line, sizeof(line), "IP %s", net::WifiIp().c_str());
            oled::Text(0, 13, line);
            std::snprintf(line, sizeof(line), "WiFi %d dBm", net::WifiRssi());
            oled::Text(0, 26, line);
        } else {
            oled::Text(0, 13, "WiFi connecting...");
        }

        oled::Text(0, 39, clk::Format("%d-%m-%Y").c_str());
        oled::Text(0, 52, clk::Format("%H:%M:%S").c_str());
        oled::Show();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main(void) {
    PrintBoardInfo();

    if (oled::Init(kI2cSda, kI2cScl)) {
        oled::Text(0, 0, "Smart Controller");
        oled::Text(0, 13, "Booting...");
        oled::Show();
        xTaskCreate(DisplayTask, "display", 4096, nullptr, 3, nullptr);
    }

    net::WifiStart(WIFI_SSID, WIFI_PASS);
    clk::TimeSyncStart();

    while (true) {
        int64_t uptime_s = esp_timer_get_time() / 1000000;
        if (net::WifiIsConnected()) {
            ESP_LOGI(TAG, "IP %s | RSSI %d dBm | %s | up %" PRId64 " s",
                     net::WifiIp().c_str(), net::WifiRssi(),
                     clk::TimeNow().c_str(), uptime_s);
        } else {
            ESP_LOGI(TAG, "WiFi connecting... | up %" PRId64 " s", uptime_s);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
