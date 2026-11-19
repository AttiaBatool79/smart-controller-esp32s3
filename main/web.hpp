#pragma once
// Web server on port 80 (dashboard + REST API):
//   GET  /                         dashboard page
//   GET  /api/status               live JSON (same as MQTT status)
//   GET  /api/events               last 20 events
//   POST /api/relay?ch=1&on=1      switch a relay
//   POST /api/schedule?ch=1&en=1&on=18:00&off=23:00
//   POST /api/wifi    body: ssid=..&pass=..   (saves + reboots)
//   POST /api/ota     body: firmware .bin     (update + reboot)
//   POST /api/reboot

namespace web {
bool Start();
}
