#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include "i2c_bus.h"
#include "oled_042.h"

void app_main(void)
{
    ESP_ERROR_CHECK(app_i2c_init());

    oled_042_t oled;
    ESP_ERROR_CHECK(oled_042_init(&oled, app_i2c_port(), OLED042_I2C_ADDR));

    while (1) {
        oled_042_clear(&oled);
        oled_042_draw_text(&oled, 0, 0, "ESP32-C3", true);
        oled_042_draw_text(&oled, 0, 10, "OLED OK", true);
        oled_042_draw_text(&oled, 0, 20, "I2C BUS OK", true);
        oled_042_draw_text(&oled, 0, 30, "0x23 57 68", true);
        ESP_ERROR_CHECK(oled_042_update(&oled));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}