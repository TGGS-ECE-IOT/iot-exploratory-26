#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include "i2c_bus.h"
#include "oled_042.h"
#include "mpu6050.h"

void app_main(void)
{
    ESP_ERROR_CHECK(app_i2c_init());

    oled_042_t oled;
    mpu6050_t mpu;
    mpu6050_data_t data;

    ESP_ERROR_CHECK(oled_042_init(&oled, app_i2c_port(), OLED042_I2C_ADDR));
    ESP_ERROR_CHECK(mpu6050_init(&mpu, app_i2c_port(), MPU6050_I2C_ADDR));

    while (1) {
        esp_err_t err = mpu6050_read(&mpu, &data);

        char line[24];

        oled_042_clear(&oled);
        oled_042_draw_text(&oled, 0, 0, "MPU6050", true);

        if (err == ESP_OK) {
            snprintf(line, sizeof(line), "AX:% .2f", data.ax);
            oled_042_draw_text(&oled, 0, 10, line, true);

            snprintf(line, sizeof(line), "AY:% .2f", data.ay);
            oled_042_draw_text(&oled, 0, 20, line, true);

            snprintf(line, sizeof(line), "AZ:% .2f", data.az);
            oled_042_draw_text(&oled, 0, 30, line, true);
        } else {
            oled_042_draw_text(&oled, 0, 12, "READ ERROR", true);
        }

        ESP_ERROR_CHECK(oled_042_update(&oled));
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}