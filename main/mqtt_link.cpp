#include "mqtt_link.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "config.hpp"
#include "esp_log.h"
#include "events.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "io.hpp"
#include "mqtt_client.h"
#include "status.hpp"
#include "wifi.hpp"

namespace mqtt {
namespace {

const char* TAG = "mqtt";
esp_mqtt_client_handle_t s_client = nullptr;
bool s_connected = false;
std::string s_status_topic;
std::string s_cmd_topic;

// "1 on" / "2 off" / "1 toggle"
void HandleCommand(const std::string& cmd) {
    int ch = 0;
    char action[12] = {};
    if (std::sscanf(cmd.c_str(), "%d %11s", &ch, action) != 2 || ch < 1 ||
        ch > io::kRelayCount) {
        ESP_LOGW(TAG, "Unknown command \"%s\" (use: 1 on | 1 off | 1 toggle)",
                 cmd.c_str());
        return;
    }
    const int i = ch - 1;
    if (std::strcmp(action, "on") == 0) {
        io::RelaySet(i, true, "mqtt");
    } else if (std::strcmp(action, "off") == 0) {
        io::RelaySet(i, false, "mqtt");
    } else if (std::strcmp(action, "toggle") == 0) {
        io::RelaySet(i, !io::RelayGet(i), "mqtt");
    } else {
        return;
    }
    PublishNow();
}

void OnEvent(void*, esp_event_base_t, int32_t id, void* data) {
    auto* e = static_cast<esp_mqtt_event_handle_t>(data);
    switch (id) {
        case MQTT_EVENT_CONNECTED:
            s_connected = true;
            esp_mqtt_client_subscribe(s_client, s_cmd_topic.c_str(), 1);
            events::Add("MQTT connected");
            PublishNow();
            break;
        case MQTT_EVENT_DISCONNECTED:
            if (s_connected) events::Add("MQTT disconnected");
            s_connected = false;
            break;
        case MQTT_EVENT_DATA:
            if (std::string(e->topic, e->topic_len) == s_cmd_topic) {
                HandleCommand(std::string(e->data, e->data_len));
            }
            break;
        default:
            break;
    }
}

void PublishTask(void*) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(cfg::kMqttPublishSec * 1000));
        PublishNow();
    }
}

}  // namespace

void Start() {
    if (!cfg::kMqttEnabled || s_client != nullptr) return;
    const std::string base = std::string(cfg::kMqttTopicRoot) + "/" + net::DeviceId();
    s_status_topic = base + "/status";
    s_cmd_topic = base + "/cmd";
    static std::string client_id = "smartctl-" + net::DeviceId();

    esp_mqtt_client_config_t c{};
    c.broker.address.uri = cfg::kMqttBroker;
    c.credentials.client_id = client_id.c_str();
    c.session.keepalive = 30;
    s_client = esp_mqtt_client_init(&c);
    esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, &OnEvent, nullptr);
    esp_mqtt_client_start(s_client);  // connects by itself once WiFi is up
    xTaskCreate(PublishTask, "mqtt_pub", 4096, nullptr, 3, nullptr);
    ESP_LOGI(TAG, "Topics: %s | %s", s_status_topic.c_str(), s_cmd_topic.c_str());
}

bool IsConnected() { return s_connected; }

void PublishNow() {
    if (!s_connected) return;
    const std::string json = status::Json();
    esp_mqtt_client_publish(s_client, s_status_topic.c_str(), json.c_str(), 0, 0,
                            1 /* retain: new subscribers get the last state */);
}

}  // namespace mqtt
