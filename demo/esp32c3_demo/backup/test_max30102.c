#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"

#include "i2c_bus.h"
#include "oled_042.h"
#include "max3010x.h"

void app_main(void)
{
    ESP_ERROR_CHECK(app_i2c_init());

    oled_042_t oled;
    max3010x_t max;

    ESP_ERROR_CHECK(oled_042_init(&oled, app_i2c_port(), OLED042_I2C_ADDR));
    ESP_ERROR_CHECK(max3010x_init(&max, app_i2c_port(), MAX3010X_I2C_ADDR));

    while (1) {
        max3010x_data_t data = {0};
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        esp_err_t err = max3010x_read(&max, &data, now_ms);

        char line[24];

        oled_042_clear(&oled);
        oled_042_draw_text(&oled, 0, 0, "MAX30102", true);

        if (err == ESP_OK) {
            snprintf(line, sizeof(line), "IR:%5lu", (unsigned long)(data.ir / 10UL));
            oled_042_draw_text(&oled, 0, 10, line, true);

            snprintf(line, sizeof(line), "RD:%5lu", (unsigned long)(data.red / 10UL));
            oled_042_draw_text(&oled, 0, 20, line, true);

            oled_042_draw_text(&oled, 0, 30, data.finger_present ? "FINGER" : "NO FINGER", true);
        } else if (err == ESP_ERR_NOT_FOUND) {
            oled_042_draw_text(&oled, 0, 12, "NO SAMPLE", true);
        } else {
            oled_042_draw_text(&oled, 0, 12, "READ ERR", true);
        }

        ESP_ERROR_CHECK(oled_042_update(&oled));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
