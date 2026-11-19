#pragma once
// WiFi manager.
//  - WiFi name/password are saved in NVS (set from the web page).
//  - First boot with nothing saved: uses secrets.h if present.
//  - No WiFi saved, or not connected after 30 s: opens its own access point
//    "SmartCtl-XXXX" (password in config.hpp). Join it and open
//    http://192.168.4.1 to enter your WiFi.

#include <string>

namespace net {
void WifiStart();
bool WifiIsConnected();
std::string WifiIp();
std::string WifiSsid();      // network we try to join
int WifiRssi();              // signal strength in dBm
bool ApActive();             // setup access point is on
std::string ApSsid();
std::string DeviceId();      // last 6 hex digits of MAC, e.g. "1B6774"
void SaveCredentials(const std::string& ssid, const std::string& pass);
}  // namespace net
