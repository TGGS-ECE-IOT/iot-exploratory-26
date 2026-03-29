#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_err.h"

#include "i2c_bus.h"
#include "oled_042.h"

#define BUTTON_GPIO          GPIO_NUM_1
#define BUTTON_ACTIVE_LEVEL  0
#define BUTTON_DEBOUNCE_MS   25

static void button_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

static bool button_pressed_event(void)
{
    static int last_raw = 1;
    static int stable_state = 1;
    static TickType_t last_change_tick = 0;
    static bool press_armed = false;

    int raw = gpio_get_level(BUTTON_GPIO);
    TickType_t now = xTaskGetTickCount();

    if (raw != last_raw) {
        last_raw = raw;
        last_change_tick = now;
    }

    if ((now - last_change_tick) < pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS)) {
        return false;
    }

    if (stable_state != raw) {
        stable_state = raw;

        if (stable_state == BUTTON_ACTIVE_LEVEL) {
            press_armed = true;
        } else {
            if (press_armed) {
                press_armed = false;
                return true;
            }
        }
    }

    return false;
}

void app_main(void)
{
    ESP_ERROR_CHECK(app_i2c_init());
    button_init();

    oled_042_t oled;
    ESP_ERROR_CHECK(oled_042_init(&oled, app_i2c_port(), OLED042_I2C_ADDR));

    int press_count = 0;

    while (1) {
        int raw = gpio_get_level(BUTTON_GPIO);
        bool pressed = (raw == BUTTON_ACTIVE_LEVEL);

        if (button_pressed_event()) {
            press_count++;
        }

        char line[24];

        oled_042_clear(&oled);
        oled_042_draw_text(&oled, 0, 0, "BUTTON TEST", true);

        oled_042_draw_text(&oled, 0, 10, pressed ? "PRESSED" : "RELEASED", true);

        snprintf(line, sizeof(line), "RAW:%d", raw);
        oled_042_draw_text(&oled, 0, 20, line, true);

        snprintf(line, sizeof(line), "COUNT:%d", press_count);
        oled_042_draw_text(&oled, 0, 30, line, true);

        ESP_ERROR_CHECK(oled_042_update(&oled));
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
