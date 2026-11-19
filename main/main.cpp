#include <cinttypes>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "smart";

extern "C" void app_main(void) {
    // 1) Board ki info ek dafa print karo
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    esp_flash_get_size(nullptr, &flash_size);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "=== Smart Controller v0.1 ===");
    ESP_LOGI(TAG, "Chip: ESP32-S3, %d cores, revision v%d.%d", chip.cores,
             chip.revision / 100, chip.revision % 100);
    ESP_LOGI(TAG, "Flash: %" PRIu32 " MB", flash_size / (1024 * 1024));
    ESP_LOGI(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
             mac[2], mac[3], mac[4], mac[5]);

    // 2) Har 2 second "zinda hoon" message
    while (true) {
        int64_t uptime_s = esp_timer_get_time() / 1000000;
        ESP_LOGI(TAG, "Uptime: %" PRId64 " s | Free RAM: %" PRIu32 " bytes",
                 uptime_s, esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}