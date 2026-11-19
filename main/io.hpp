#pragma once
// KinCony KC868-A2v3 on-board I/O: 2 relays, 2 digital inputs, and the
// ESP32-S3 internal temperature sensor.

namespace io {

constexpr int kRelayCount = 2;
constexpr int kInputCount = 2;

void Init();
void RelaySet(int index, bool on);  // index 0 or 1
bool RelayGet(int index);
bool InputActive(int index);        // true = input terminal pulled to GND
float ChipTempC();                  // chip temperature (not room temp!)

}  // namespace io
