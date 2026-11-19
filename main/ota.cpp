#include "ota.hpp"

#include <algorithm>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "events.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ota {
namespace {
const char* TAG = "ota";

void RebootTask(void*) {
    vTaskDelay(pdMS_TO_TICKS(1500));  // let the HTTP answer go out first
    esp_restart();
}
}  // namespace

esp_err_t UploadHandler(httpd_req_t* req) {
    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    if (target == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "No OTA partition (check partitions.csv)");
        return ESP_FAIL;
    }
    if (req->content_len == 0 || req->content_len > target->size) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad firmware size");
        return ESP_FAIL;
    }

    esp_ota_handle_t handle = 0;
    if (esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &handle) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Receiving %d bytes into %s", req->content_len, target->label);

    char buf[2048];
    int remaining = req->content_len;
    while (remaining > 0) {
        const int got = httpd_req_recv(req, buf, std::min<int>(remaining, sizeof(buf)));
        if (got == HTTPD_SOCK_ERR_TIMEOUT) continue;  // slow network, try again
        if (got <= 0 || esp_ota_write(handle, buf, got) != ESP_OK) {
            esp_ota_abort(handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload failed");
            return ESP_FAIL;
        }
        remaining -= got;
    }

    // esp_ota_end checks the image (magic byte, chip, SHA-256)
    if (esp_ota_end(handle) != ESP_OK || esp_ota_set_boot_partition(target) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware image");
        return ESP_FAIL;
    }
    events::Add("Firmware uploaded to %s, rebooting", target->label);
    httpd_resp_sendstr(req, "OK, rebooting into new firmware...");
    xTaskCreate(RebootTask, "reboot", 2048, nullptr, 5, nullptr);
    return ESP_OK;
}

void MarkHealthy() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        events::Add("New firmware %s confirmed OK", Version());
    }
}

const char* Version() { return esp_app_get_description()->version; }

const char* RunningPartition() { return esp_ota_get_running_partition()->label; }

}  // namespace ota
