#pragma once
// Tiny wrapper around NVS (the ESP32's key/value flash storage).
// Everything saved here survives reboot and firmware updates.

#include <cstddef>
#include <cstdint>
#include <string>

namespace storage {

void Init();  // call first, before anything else
std::string GetStr(const char* key, const std::string& fallback = "");
void SetStr(const char* key, const std::string& value);
uint8_t GetU8(const char* key, uint8_t fallback = 0);
void SetU8(const char* key, uint8_t value);
bool GetBlob(const char* key, void* out, size_t size);
void SetBlob(const char* key, const void* data, size_t size);

}  // namespace storage
