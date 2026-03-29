#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include "i2c_bus.h"
#include "oled_042.h"
#include "bh1750.h"

void app_main(void)
{
    ESP_ERROR_CHECK(app_i2c_init());

    oled_042_t oled;
    bh1750_t bh;

    ESP_ERROR_CHECK(oled_042_init(&oled, app_i2c_port(), OLED042_I2C_ADDR));
    ESP_ERROR_CHECK(bh1750_init(&bh, app_i2c_port(), BH1750_I2C_ADDR));

    while (1) {
        float lux = 0.0f;
        esp_err_t err = bh1750_read_lux(&bh, &lux);

        char line[24];

        oled_042_clear(&oled);
        oled_042_draw_text(&oled, 0, 0, "BH1750 TEST", true);

        if (err == ESP_OK) {
            snprintf(line, sizeof(line), "LUX: %.1f", lux);
            oled_042_draw_text(&oled, 0, 12, line, true);
        } else {
            oled_042_draw_text(&oled, 0, 12, "READ ERROR", true);
        }

        ESP_ERROR_CHECK(oled_042_update(&oled));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}