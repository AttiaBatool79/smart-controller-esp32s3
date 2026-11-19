#pragma once
// KinCony KC868-A2v3 on-board I/O: 2 relays, 2 digital inputs, and the
// ESP32-S3 internal temperature sensor.

namespace io {

constexpr int kRelayCount = 2;
constexpr int kInputCount = 2;

//! Set up pins, restore the relay states saved before the last reboot,
//! and start the input watcher (inputs can toggle relays, see config.hpp).
void Init();
//! index 0 or 1. `source` is shown in the event log: "web", "schedule"...
void RelaySet(int index, bool on, const char* source);
bool RelayGet(int index);
bool InputActive(int index);  // true = input terminal pulled to GND
float ChipTempC();            // chip temperature (not room temp!)

}  // namespace io
