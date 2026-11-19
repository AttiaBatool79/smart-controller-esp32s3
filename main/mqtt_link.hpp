#pragma once
// MQTT link (control from anywhere, e.g. a phone MQTT app).
//   publishes  smartctl/<id>/status   full JSON every 10 s (+ on change)
//   listens on smartctl/<id>/cmd      "1 on", "2 off", "1 toggle"
// Broker is set in config.hpp (public HiveMQ test broker by default).

namespace mqtt {
void Start();          // safe to call before WiFi is up
bool IsConnected();
void PublishNow();     // send status right away (after a relay change)
}  // namespace mqtt
