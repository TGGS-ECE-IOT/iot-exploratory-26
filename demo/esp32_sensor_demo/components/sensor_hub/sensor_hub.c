#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_err.h"
#include "nvs_flash.h"

#include "app_state.h"
#include "buzzer.h"
#include "mqtt_manager.h"
#include "sensor_hub.h"
#include "sensors.h"
#include "status.h"
#include "ui.h"
#include "wifi_manager.h"

void sensor_hub_start(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    data_mutex = xSemaphoreCreateMutex();

    sensors_init_hardware();
    buzzer_init();
    ui_init();
    ui_show_boot_splash();

    set_status("System init...");
    buzzer_play_ok_short();

    wifi_init_sta();

    xTaskCreate(buzzer_task, "buzzer_task", 2048, NULL, 5, NULL);
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(ui_task, "ui_task", 4096, NULL, 4, NULL);
    xTaskCreate(mqtt_tx_task, "mqtt_tx_task", 4096, NULL, 4, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
