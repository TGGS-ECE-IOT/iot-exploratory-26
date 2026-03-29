#include "led.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "LED";

void led_init(led_config_t *config)
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
    ESP_LOGI(TAG, "LED initialized on GPIO %d", config->gpio);
}

void led_on(led_config_t *config)
{
    gpio_set_level(config->gpio, 1);
    config->state = 1;
}

void led_off(led_config_t *config)
{
    gpio_set_level(config->gpio, 0);
    config->state = 0;
}

void led_toggle(led_config_t *config)
{
    if (config->state) {
        led_off(config);
    } else {
        led_on(config);
    }
}

uint8_t led_get_state(led_config_t *config)
{
    return config->state;
}

void led_traffic_init(led_config_t *red, led_config_t *yellow, led_config_t *green)
{
    led_init(red);
    led_init(yellow);
    led_init(green);
    led_off(red);
    led_off(yellow);
    led_off(green);
    ESP_LOGI(TAG, "Traffic light initialized");
}

void led_traffic_set(led_config_t *red, led_config_t *yellow, led_config_t *green, led_color_t color)
{
    led_off(red);
    led_off(yellow);
    led_off(green);

    switch (color) {
        case LED_RED:
            led_on(red);
            break;
        case LED_YELLOW:
            led_on(yellow);
            break;
        case LED_GREEN:
            led_on(green);
            break;
    }
}
