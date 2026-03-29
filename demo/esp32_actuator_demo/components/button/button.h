#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include "driver/gpio.h"
#include "pin_config.h"

typedef struct {
    gpio_num_t gpio;
    uint8_t active_level;
    uint32_t debounce_ms;
    int64_t last_press_time;
    uint8_t last_state;
} button_config_t;

void button_init(button_config_t *config);
uint8_t button_is_pressed(button_config_t *config);

#endif
