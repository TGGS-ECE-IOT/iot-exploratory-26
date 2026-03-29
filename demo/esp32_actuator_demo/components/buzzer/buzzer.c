#include "buzzer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BUZZER";

void buzzer_init(buzzer_config_t *config)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << config->gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);
    config->state = 0;
    gpio_set_level(config->gpio, 0);
    ESP_LOGI(TAG, "Buzzer initialized on GPIO %d", config->gpio);
}

void buzzer_on(buzzer_config_t *config)
{
    gpio_set_level(config->gpio, 1);
    config->state = 1;
}

void buzzer_off(buzzer_config_t *config)
{
    gpio_set_level(config->gpio, 0);
    config->state = 0;
}

void buzzer_beep(buzzer_config_t *config, uint32_t duration_ms)
{
    buzzer_on(config);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    buzzer_off(config);
}

uint8_t buzzer_get_state(buzzer_config_t *config)
{
    return config->state;
}
