#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "driver/gpio.h"
#include "pin_config.h"

typedef struct {
    gpio_num_t gpio;
    uint8_t state;
} led_config_t;

typedef enum {
    LED_RED,
    LED_YELLOW,
    LED_GREEN
} led_color_t;

void led_init(led_config_t *config);
void led_on(led_config_t *config);
void led_off(led_config_t *config);
void led_toggle(led_config_t *config);
uint8_t led_get_state(led_config_t *config);

void led_traffic_init(led_config_t *red, led_config_t *yellow, led_config_t *green);
void led_traffic_set(led_config_t *red, led_config_t *yellow, led_config_t *green, led_color_t color);

#endif
