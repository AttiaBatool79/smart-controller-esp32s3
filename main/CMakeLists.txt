idf_component_register(
    SRCS "main.cpp" "storage.cpp" "events.cpp" "io.cpp" "oled.cpp"
         "wifi.cpp" "time_sync.cpp" "schedule.cpp" "status.cpp"
         "mqtt_link.cpp" "ota.cpp" "web.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_http_server mqtt
             app_update esp_app_format esp_timer spi_flash esp_driver_gpio
             esp_driver_i2c esp_driver_tsens)
