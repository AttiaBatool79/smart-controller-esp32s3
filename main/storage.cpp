#include "storage.hpp"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace storage {
namespace {
constexpr const char* kNamespace = "smart";
}

void Init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW("storage", "NVS format changed, erasing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

std::string GetStr(const char* key, const std::string& fallback) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return fallback;
    size_t len = 0;
    std::string out = fallback;
    if (nvs_get_str(h, key, nullptr, &len) == ESP_OK && len > 0) {
        out.resize(len);
        nvs_get_str(h, key, out.data(), &len);
        out.resize(len - 1);  // drop the trailing '\0'
    }
    nvs_close(h);
    return out;
}

void SetStr(const char* key, const std::string& value) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, key, value.c_str());
    nvs_commit(h);
    nvs_close(h);
}

uint8_t GetU8(const char* key, uint8_t fallback) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return fallback;
    uint8_t v = fallback;
    if (nvs_get_u8(h, key, &v) != ESP_OK) v = fallback;
    nvs_close(h);
    return v;
}

void SetU8(const char* key, uint8_t value) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, key, value);
    nvs_commit(h);
    nvs_close(h);
}

bool GetBlob(const char* key, void* out, size_t size) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = size;
    const bool ok = nvs_get_blob(h, key, out, &len) == ESP_OK && len == size;
    nvs_close(h);
    return ok;
}

void SetBlob(const char* key, const void* data, size_t size) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, key, data, size);
    nvs_commit(h);
    nvs_close(h);
}

}  // namespace storage
