#ifndef RELAY_H
#define RELAY_H

#include <stdint.h>
#include "driver/gpio.h"
#include "pin_config.h"

typedef struct {
    gpio_num_t gpio;
    uint8_t active_level;
    uint8_t state;
} relay_config_t;

void relay_init(relay_config_t *config);
void relay_on(relay_config_t *config);
void relay_off(relay_config_t *config);
void relay_toggle(relay_config_t *config);
uint8_t relay_get_state(relay_config_t *config);

#endif
