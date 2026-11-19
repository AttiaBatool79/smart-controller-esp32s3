#pragma once
// Firmware update over WiFi: upload a new .bin from the web page.
// Uses two app slots (ota_0 / ota_1). If the new firmware does not reach
// "healthy" (MarkHealthy), the bootloader rolls back to the old one.

#include "esp_http_server.h"

namespace ota {
esp_err_t UploadHandler(httpd_req_t* req);  // POST /api/ota (raw .bin body)
void MarkHealthy();                         // call once the app works (WiFi up)
const char* Version();                      // from version.txt
const char* RunningPartition();
}  // namespace ota
