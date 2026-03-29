idf_component_register(
    SRCS
        "sensor_hub.c"
        "app_state.c"
        "utils.c"
        "status.c"
        "buzzer.c"
        "wifi_manager.c"
        "mqtt_manager.c"
        "sensors.c"
        "oled_driver.c"
        "splash_assets.c"
        "ui.c"
    INCLUDE_DIRS "include" "${IDF_PATH}/components/esp_adc/include"
    REQUIRES esp_adc esp_event esp_http_server esp_netif esp_timer esp_wifi mqtt nvs_flash driver lwip u8g2
)
