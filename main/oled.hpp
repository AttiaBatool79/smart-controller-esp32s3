#pragma once
// Minimal SSD1306 128x64 OLED driver over I2C (ESP-IDF new i2c_master API).
// Draw into a RAM buffer with Clear()/Text(), then call Show() to send it.

#include <cstdint>

namespace oled {

constexpr int kWidth = 128;
constexpr int kHeight = 64;

//! Init I2C bus + display. Returns false if no OLED answers at 0x3C/0x3D.
bool Init(int sda_pin, int scl_pin);

void Clear();
//! Draw text with its top-left corner at (x, y) pixels. 6x11 font,
//! so one line is ~12 px tall and holds 21 characters.
void Text(int x, int y, const char* text);
//! Send the RAM buffer to the screen.
void Show();

}  // namespace oled
