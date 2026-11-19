#include "wifi.hpp"

#include <cstdio>
#include <cstring>

#include "config.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "events.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "storage.hpp"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif

namespace net {
namespace {

const char* TAG = "wifi";
EventGroupHandle_t s_events = nullptr;
constexpr EventBits_t kConnectedBit = BIT0;
char s_ip[16] = "0.0.0.0";
std::string s_ssid;
bool s_ap_active = false;
esp_timer_handle_t s_fallback_timer = nullptr;
esp_timer_handle_t s_retry_timer = nullptr;

void StartAp() {
    if (s_ap_active) return;
    wifi_config_t ap{};
    const std::string name = ApSsid();
    strlcpy(reinterpret_cast<char*>(ap.ap.ssid), name.c_str(), sizeof(ap.ap.ssid));
    ap.ap.ssid_len = name.size();
    strlcpy(reinterpret_cast<char*>(ap.ap.password), cfg::kApPassword,
            sizeof(ap.ap.password));
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(s_ssid.empty() ? WIFI_MODE_AP : WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    s_ap_active = true;
    events::Add("Setup WiFi \"%s\" open -> http://192.168.4.1", name.c_str());
}

void StopAp() {
    if (!s_ap_active) return;
    esp_wifi_set_mode(WIFI_MODE_STA);
    s_ap_active = false;
    ESP_LOGI(TAG, "Setup access point closed");
}

void FallbackTimerCb(void*) {
    if (!WifiIsConnected()) {
        ESP_LOGW(TAG, "Not connected after %d s, opening setup AP",
                 cfg::kApFallbackAfterSec);
        StartAp();
    }
}

void RetryTimerCb(void*) { esp_wifi_connect(); }

void OnEvent(void*, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (!s_ssid.empty()) esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        auto* e = static_cast<wifi_event_sta_disconnected_t*>(data);
        const bool was_connected = WifiIsConnected();
        xEventGroupClearBits(s_events, kConnectedBit);
        std::strcpy(s_ip, "0.0.0.0");
        if (was_connected) events::Add("WiFi lost (reason %d)", e->reason);
        if (s_ssid.empty()) return;
        // While the setup AP is open, retry slowly: STA scanning disturbs
        // phones connected to the AP.
        if (s_ap_active) {
            esp_timer_start_once(s_retry_timer, 15 * 1000 * 1000);
        } else {
            esp_timer_start_once(s_retry_timer, 2 * 1000 * 1000);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* e = static_cast<ip_event_got_ip_t*>(data);
        std::snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_events, kConnectedBit);
        events::Add("WiFi connected, IP %s", s_ip);
        StopAp();
    }
}

}  // namespace

void WifiStart() {
    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();  // 192.168.4.1 when AP is on

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));  // we use our own NVS keys
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &OnEvent, nullptr, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &OnEvent, nullptr, nullptr));

    esp_timer_create_args_t t{};
    t.callback = FallbackTimerCb;
    t.name = "wifi_fallback";
    esp_timer_create(&t, &s_fallback_timer);
    t.callback = RetryTimerCb;
    t.name = "wifi_retry";
    esp_timer_create(&t, &s_retry_timer);

    // Saved credentials first, then secrets.h (first boot only).
    s_ssid = storage::GetStr("wifi_ssid", WIFI_SSID);
    const std::string pass = storage::GetStr("wifi_pass", WIFI_PASS);

    if (s_ssid.empty()) {
        ESP_LOGW(TAG, "No WiFi saved");
        StartAp();
        ESP_ERROR_CHECK(esp_wifi_start());
        return;
    }

    wifi_config_t sta{};
    strlcpy(reinterpret_cast<char*>(sta.sta.ssid), s_ssid.c_str(),
            sizeof(sta.sta.ssid));
    strlcpy(reinterpret_cast<char*>(sta.sta.password), pass.c_str(),
            sizeof(sta.sta.password));
    sta.sta.threshold.authmode = pass.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_timer_start_once(s_fallback_timer,
                         static_cast<uint64_t>(cfg::kApFallbackAfterSec) * 1000000);
    ESP_LOGI(TAG, "Connecting to \"%s\"...", s_ssid.c_str());
}

bool WifiIsConnected() {
    return s_events && (xEventGroupGetBits(s_events) & kConnectedBit);
}

std::string WifiIp() { return s_ip; }
std::string WifiSsid() { return s_ssid; }
bool ApActive() { return s_ap_active; }

int WifiRssi() {
    wifi_ap_record_t ap{};
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;
}

std::string DeviceId() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char id[8];
    std::snprintf(id, sizeof(id), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return id;
}

std::string ApSsid() {
    return std::string(cfg::kApSsidPrefix) + DeviceId().substr(2);
}

void SaveCredentials(const std::string& ssid, const std::string& pass) {
    storage::SetStr("wifi_ssid", ssid);
    storage::SetStr("wifi_pass", pass);
    events::Add("WiFi settings saved for \"%s\"", ssid.c_str());
}

}  // namespace net
