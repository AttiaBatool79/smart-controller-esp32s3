#include <cinttypes>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "secrets.h"
#include "time_sync.hpp"
#include "wifi.hpp"

static const char* TAG = "smart";

static void PrintBoardInfo() {
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    uint32_t flash_size = 0;
    esp_flash_get_size(nullptr, &flash_size);
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "=== Smart Controller v0.2 ===");
    ESP_LOGI(TAG, "Chip: ESP32-S3, %d cores, rev v%d.%d", chip.cores,
             chip.revision / 100, chip.revision % 100);
    ESP_LOGI(TAG, "Flash: %" PRIu32 " MB", flash_size / (1024 * 1024));
    ESP_LOGI(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
             mac[2], mac[3], mac[4], mac[5]);
}

extern "C" void app_main(void) {
    PrintBoardInfo();
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