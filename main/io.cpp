#include "io.hpp"

#include "driver/gpio.h"
#include "driver/temperature_sensor.h"
#include "esp_log.h"

namespace io {
namespace {

const char* TAG = "io";
// Pins from the KC868-A2v3 schematic
constexpr gpio_num_t kRelayPins[kRelayCount] = {GPIO_NUM_40, GPIO_NUM_39};
constexpr gpio_num_t kInputPins[kInputCount] = {GPIO_NUM_16, GPIO_NUM_17};

bool s_relay[kRelayCount] = {false, false};
temperature_sensor_handle_t s_temp = nullptr;

}  // namespace

void Init() {
    for (int i = 0; i < kRelayCount; ++i) {
        gpio_reset_pin(kRelayPins[i]);
        gpio_set_direction(kRelayPins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(kRelayPins[i], 0);  // start OFF
    }
    for (int i = 0; i < kInputCount; ++i) {
        gpio_reset_pin(kInputPins[i]);
        gpio_set_direction(kInputPins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(kInputPins[i], GPIO_PULLUP_ONLY);
    }

    temperature_sensor_config_t cfg{};
    cfg.range_min = -10;
    cfg.range_max = 80;
    cfg.clk_src = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT;
    if (temperature_sensor_install(&cfg, &s_temp) == ESP_OK) {
        temperature_sensor_enable(s_temp);
    } else {
        s_temp = nullptr;
    }
    ESP_LOGI(TAG, "Relays GPIO40/39, inputs GPIO16/17 ready");
}

void RelaySet(int index, bool on) {
    if (index < 0 || index >= kRelayCount) return;
    gpio_set_level(kRelayPins[index], on ? 1 : 0);
    s_relay[index] = on;
    ESP_LOGI(TAG, "Relay %d -> %s", index + 1, on ? "ON" : "OFF");
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
    if (s_temp == nullptr || temperature_sensor_get_celsius(s_temp, &t) != ESP_OK) {
        return -99.0f;
    }
    return t;
}

}  // namespace io
