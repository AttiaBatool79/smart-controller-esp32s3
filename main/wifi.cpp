#include "wifi.hpp"

#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

namespace net {
namespace {

const char* TAG = "wifi";
EventGroupHandle_t s_events = nullptr;
constexpr EventBits_t kConnectedBit = BIT0;
char s_ip[16] = "0.0.0.0";

void OnEvent(void*, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        auto* e = static_cast<wifi_event_sta_disconnected_t*>(data);
        xEventGroupClearBits(s_events, kConnectedBit);
        std::strcpy(s_ip, "0.0.0.0");
        ESP_LOGW(TAG, "Disconnected (reason %d), retrying...", e->reason);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* e = static_cast<ip_event_got_ip_t*>(data);
        std::snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
        ESP_LOGI(TAG, "Connected! IP: %s", s_ip);
        xEventGroupSetBits(s_events, kConnectedBit);
    }
}

}  // namespace

void WifiStart(const char* ssid, const char* pass) {
    // NVS: WiFi driver apni settings yahan rakhta hai
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &OnEvent, nullptr, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &OnEvent, nullptr, nullptr));

    wifi_config_t cfg{};
    strlcpy(reinterpret_cast<char*>(cfg.sta.ssid), ssid, sizeof(cfg.sta.ssid));
    strlcpy(reinterpret_cast<char*>(cfg.sta.password), pass,
            sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Connecting to \"%s\"...", ssid);
}

bool WifiIsConnected() {
    return s_events && (xEventGroupGetBits(s_events) & kConnectedBit);
}

std::string WifiIp() { return s_ip; }

int WifiRssi() {
    wifi_ap_record_t ap{};
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;
}

}  // namespace net