#include "oled.hpp"

#include <cstring>
#include <initializer_list>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "font6x11.hpp"

namespace oled {
namespace {

const char* TAG = "oled";
constexpr int kTimeoutMs = 100;

i2c_master_bus_handle_t s_bus = nullptr;
i2c_master_dev_handle_t s_dev = nullptr;
// 1 control byte (0x40 = "data follows") + 8 pages x 128 columns
uint8_t s_buf[1 + kWidth * kHeight / 8];
uint8_t* const s_fb = s_buf + 1;

bool Command(const uint8_t* cmds, size_t len) {
    uint8_t tx[32];
    tx[0] = 0x00;  // control byte: commands follow
    std::memcpy(tx + 1, cmds, len);
    return i2c_master_transmit(s_dev, tx, len + 1, kTimeoutMs) == ESP_OK;
}

void SetPixel(int x, int y) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
    s_fb[(y / 8) * kWidth + x] |= static_cast<uint8_t>(1 << (y % 8));
}

}  // namespace

bool Init(int sda_pin, int scl_pin) {
    i2c_master_bus_config_t bus_cfg{};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = static_cast<gpio_num_t>(sda_pin);
    bus_cfg.scl_io_num = static_cast<gpio_num_t>(scl_pin);
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&bus_cfg, &s_bus) != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed");
        return false;
    }

    // Most SSD1306 modules answer at 0x3C, some at 0x3D.
    uint16_t addr = 0;
    for (uint16_t a : {0x3C, 0x3D}) {
        if (i2c_master_probe(s_bus, a, kTimeoutMs) == ESP_OK) {
            addr = a;
            break;
        }
    }
    if (addr == 0) {
        ESP_LOGE(TAG, "No OLED found at 0x3C/0x3D (SDA=%d SCL=%d)", sda_pin,
                 scl_pin);
        return false;
    }

    i2c_device_config_t dev_cfg{};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = addr;
    dev_cfg.scl_speed_hz = 400000;
    if (i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev) != ESP_OK) {
        ESP_LOGE(TAG, "Add device failed");
        return false;
    }

    // SSD1306 128x64 power-up sequence (datasheet / app note).
    const uint8_t init[] = {
        0xAE,        // display off
        0xD5, 0x80,  // clock divide
        0xA8, 0x3F,  // multiplex = 64 rows
        0xD3, 0x00,  // display offset 0
        0x40,        // start line 0
        0x8D, 0x14,  // charge pump ON
        0x20, 0x00,  // horizontal addressing mode
        0xA1,        // flip horizontally
        0xC8,        // flip vertically
        0xDA, 0x12,  // COM pins config
        0x81, 0xCF,  // contrast
        0xD9, 0xF1,  // pre-charge
        0xDB, 0x40,  // VCOMH
        0xA4,        // show RAM content
        0xA6,        // normal (not inverted)
        0xAF,        // display ON
    };
    if (!Command(init, sizeof(init))) {
        ESP_LOGE(TAG, "Init commands failed");
        return false;
    }
    s_buf[0] = 0x40;
    Clear();
    Show();
    ESP_LOGI(TAG, "SSD1306 ready at 0x%02X", addr);
    return true;
}

void Clear() { std::memset(s_fb, 0, kWidth * kHeight / 8); }

void Text(int x, int y, const char* text) {
    for (; *text != '\0'; ++text) {
        char c = *text;
        if (c < font::kFirst || c > font::kLast) c = '?';
        const uint16_t* cols = font::kData[c - font::kFirst];
        for (int cx = 0; cx < font::kWidth; ++cx) {
            for (int cy = 0; cy < font::kHeight; ++cy) {
                if (cols[cx] & (1u << cy)) SetPixel(x + cx, y + cy);
            }
        }
        x += font::kWidth;
        if (x > kWidth - font::kWidth) break;  // no wrap: cut the line
    }
}

void Show() {
    if (s_dev == nullptr) return;
    const uint8_t window[] = {0x21, 0, kWidth - 1,       // columns 0..127
                              0x22, 0, kHeight / 8 - 1};  // pages 0..7
    Command(window, sizeof(window));
    i2c_master_transmit(s_dev, s_buf, sizeof(s_buf), kTimeoutMs);
}

}  // namespace oled
