#include "io.hpp"

#include <cstdio>

#include "config.hpp"
#include "driver/gpio.h"
#include "driver/temperature_sensor.h"
#include "esp_log.h"
#include "events.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage.hpp"

namespace io {
namespace {

const char* TAG = "io";
// Pins from the KC868-A2v3 schematic
constexpr gpio_num_t kRelayPins[kRelayCount] = {GPIO_NUM_40, GPIO_NUM_39};
constexpr gpio_num_t kInputPins[kInputCount] = {GPIO_NUM_16, GPIO_NUM_17};

bool s_relay[kRelayCount] = {false, false};
temperature_sensor_handle_t s_temp = nullptr;

void RelayKey(int index, char* key, size_t size) {
    std::snprintf(key, size, "relay%d", index + 1);
}

// Watches the inputs every 20 ms. A press (idle -> active, stable for
// 3 reads = 60 ms debounce) toggles the matching relay.
void InputTask(void*) {
    bool stable[kInputCount] = {};
    int count[kInputCount] = {};
    for (int i = 0; i < kInputCount; ++i) stable[i] = InputActive(i);

    while (true) {
        for (int i = 0; i < kInputCount; ++i) {
            const bool now = InputActive(i);
            if (now == stable[i]) {
                count[i] = 0;
                continue;
            }
            if (++count[i] >= 3) {
                stable[i] = now;
                count[i] = 0;
                events::Add("Input %d %s", i + 1, now ? "ACTIVE" : "idle");
                if (now && cfg::kInputsToggleRelays && i < kRelayCount) {
                    RelaySet(i, !RelayGet(i), "input");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

}  // namespace

void Init() {
    for (int i = 0; i < kInputCount; ++i) {
        gpio_reset_pin(kInputPins[i]);
        gpio_set_direction(kInputPins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(kInputPins[i], GPIO_PULLUP_ONLY);
    }
    for (int i = 0; i < kRelayCount; ++i) {
        gpio_reset_pin(kRelayPins[i]);
        gpio_set_direction(kRelayPins[i], GPIO_MODE_OUTPUT);
        char key[12];
        RelayKey(i, key, sizeof(key));
        s_relay[i] = storage::GetU8(key, 0) != 0;  // restore last state
        gpio_set_level(kRelayPins[i], s_relay[i] ? 1 : 0);
    }

    temperature_sensor_config_t tcfg{};
    tcfg.range_min = -10;
    tcfg.range_max = 80;
    tcfg.clk_src = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT;
    if (temperature_sensor_install(&tcfg, &s_temp) == ESP_OK) {
        temperature_sensor_enable(s_temp);
    } else {
        s_temp = nullptr;
    }

    xTaskCreate(InputTask, "inputs", 3072, nullptr, 4, nullptr);
    ESP_LOGI(TAG, "Relays restored: R1=%s R2=%s", s_relay[0] ? "ON" : "OFF",
             s_relay[1] ? "ON" : "OFF");
}

void RelaySet(int index, bool on, const char* source) {
    if (index < 0 || index >= kRelayCount) return;
    gpio_set_level(kRelayPins[index], on ? 1 : 0);
    if (s_relay[index] == on) return;  // no change: no log, no flash write
    s_relay[index] = on;
    char key[12];
    RelayKey(index, key, sizeof(key));
    storage::SetU8(key, on ? 1 : 0);
    events::Add("Relay %d %s (%s)", index + 1, on ? "ON" : "OFF", source);
}

bool RelayGet(int index) {
    return index >= 0 && index < kRelayCount && s_relay[index];
}

bool InputActive(int index) {
    if (index < 0 || index >= kInputCount) return false;
    return gpio_get_level(kInputPins[index]) == 0;  // active low
}

float ChipTempC() {
    float t = 0;
    if (s_temp == nullptr ||
        temperature_sensor_get_celsius(s_temp, &t) != ESP_OK) {
        return -99.0f;
    }
    return t;
}

}  // namespace io
