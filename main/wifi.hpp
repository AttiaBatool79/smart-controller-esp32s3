#pragma once
#include <string>

namespace net {
void WifiStart(const char* ssid, const char* pass);
bool WifiIsConnected();
std::string WifiIp();
int WifiRssi();  // signal strength in dBm
}  // namespace net