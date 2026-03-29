#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"
#include "pin_config.h"

typedef struct {
    gpio_num_t gpio;
    mcpwm_timer_handle_t timer;
    mcpwm_oper_handle_t oper;
    mcpwm_cmpr_handle_t comparator;
    mcpwm_gen_handle_t generator;
} servo_config_t;

void servo_init(servo_config_t *config);
void servo_set_angle(servo_config_t *config, uint32_t angle);

#endif
