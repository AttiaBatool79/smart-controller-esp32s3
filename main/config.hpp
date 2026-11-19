#pragma once
// All project settings in one place.

namespace cfg {

// --- Board pins (KinCony KC868-A2v3) ---
constexpr int kI2cSda = 48;
constexpr int kI2cScl = 47;

// --- WiFi setup access point (used when no WiFi is saved / reachable) ---
constexpr const char* kApSsidPrefix = "SmartCtl-";   // + last 4 MAC digits
constexpr const char* kApPassword = "smart1234";     // min 8 characters
constexpr int kApFallbackAfterSec = 30;              // STA fails this long -> AP

// --- Time ---
constexpr const char* kTimezone = "PKT-5";           // Pakistan, UTC+5
constexpr const char* kNtpServer = "pool.ntp.org";

// --- MQTT (public test broker: anyone can read/write these topics!) ---
constexpr bool kMqttEnabled = true;
constexpr const char* kMqttBroker = "mqtt://broker.hivemq.com";
constexpr const char* kMqttTopicRoot = "smartctl";   // smartctl/<id>/status|cmd
constexpr int kMqttPublishSec = 10;

// --- Local control ---
// Input 1 pulled to GND toggles Relay 1, input 2 toggles relay 2
// (like a wall push-button).
constexpr bool kInputsToggleRelays = true;

}  // namespace cfg
