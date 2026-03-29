#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include "driver/gpio.h"
#include "pin_config.h"

typedef struct {
    gpio_num_t gpio;
    uint8_t state;
} buzzer_config_t;

void buzzer_init(buzzer_config_t *config);
void buzzer_on(buzzer_config_t *config);
void buzzer_off(buzzer_config_t *config);
void buzzer_beep(buzzer_config_t *config, uint32_t duration_ms);
uint8_t buzzer_get_state(buzzer_config_t *config);

#endif
