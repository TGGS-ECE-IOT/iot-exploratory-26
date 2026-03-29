#include "relay.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "RELAY";

void relay_init(relay_config_t *config)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << config->gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);

    if (config->active_level > 1) {
        config->active_level = RELAY_ACTIVE_LEVEL;
    }

    config->state = 0;
    relay_off(config);
    ESP_LOGI(TAG, "Relay initialized on GPIO %d", config->gpio);
}

void relay_on(relay_config_t *config)
{
    gpio_set_direction(config->gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(config->gpio, config->active_level);
    config->state = 1;
    ESP_LOGI(TAG, "Relay ON");
}

void relay_off(relay_config_t *config)
{
#if RELAY_OFF_HIGH_Z
    gpio_set_direction(config->gpio, GPIO_MODE_INPUT);
    gpio_pullup_dis(config->gpio);
    gpio_pulldown_dis(config->gpio);
#else
    gpio_set_direction(config->gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(config->gpio, !config->active_level);
#endif
    config->state = 0;
    ESP_LOGI(TAG, "Relay OFF");
}

void relay_toggle(relay_config_t *config)
{
    if (config->state) {
        relay_off(config);
    } else {
        relay_on(config);
    }
}

uint8_t relay_get_state(relay_config_t *config)
{
    return config->state;
}
