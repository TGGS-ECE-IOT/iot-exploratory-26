#include "button.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "BUTTON";

void button_init(button_config_t *config)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << config->gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(err));
        return;
    }
    config->last_press_time = 0;
    config->last_state = 1;
    ESP_LOGI(TAG, "Button initialized on GPIO %d", config->gpio);
}

uint8_t button_is_pressed(button_config_t *config)
{
    uint8_t current_state = gpio_get_level(config->gpio);
    int64_t current_time = esp_timer_get_time() / 1000;

    if (current_state != config->last_state) {
        config->last_press_time = current_time;
        config->last_state = current_state;
        return 0;
    }

    if ((current_time - config->last_press_time) >= config->debounce_ms) {
        if (current_state == config->active_level) {
            return 1;
        }
    }
    return 0;
}
