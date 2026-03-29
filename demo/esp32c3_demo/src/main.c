#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "i2c_bus.h"
#include "sensor_task.h"
#include "ui_task.h"

// void app_main(void)
// {
//     ESP_ERROR_CHECK(app_i2c_init());

//     printf("app_main started\n");
//     fflush(stdout);

//     ESP_LOGI(TAG, "Starting sensor task...");
//     ESP_ERROR_CHECK(sensor_task_start());

//     ESP_LOGI(TAG, "Starting UI task...");
//     ESP_ERROR_CHECK(ui_task_start());

//     while (1) {
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }

void app_main(void)
{
    printf("\n\napp_main started\n");
    printf("reset reason = %d\n", (int)esp_reset_reason());
    fflush(stdout);

    ESP_ERROR_CHECK(app_i2c_init());

    printf("starting sensor task...\n");
    fflush(stdout);
    ESP_ERROR_CHECK(sensor_task_start());

    printf("starting ui task...\n");
    fflush(stdout);
    ESP_ERROR_CHECK(ui_task_start());

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
