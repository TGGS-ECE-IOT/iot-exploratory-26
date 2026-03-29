#include "servo.h"
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "SERVO";

#define SERVO_TIMER_RESOLUTION_HZ 1000000
#define SERVO_PERIOD_US 20000
#define SERVO_MIN_PULSE_US 500
#define SERVO_MAX_PULSE_US 2500

void servo_init(servo_config_t *config)
{
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMER_RESOLUTION_HZ,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = SERVO_PERIOD_US,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &config->timer));

    mcpwm_operator_config_t oper_config = {
        .group_id = 0,
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_config, &config->oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(config->oper, config->timer));

    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(config->oper, &comparator_config, &config->comparator));

    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = config->gpio,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(config->oper, &generator_config, &config->generator));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(
        config->generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)
    ));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(
        config->generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, config->comparator, MCPWM_GEN_ACTION_LOW)
    ));

    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(config->comparator, 1500));
    ESP_ERROR_CHECK(mcpwm_timer_enable(config->timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(config->timer, MCPWM_TIMER_START_NO_STOP));

    ESP_LOGI(TAG, "Servo initialized on GPIO %d", config->gpio);
}

void servo_set_angle(servo_config_t *config, uint32_t angle)
{
    if (angle > 180) {
        angle = 180;
    }
    uint32_t pulse_width_us = SERVO_MIN_PULSE_US + ((SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * angle) / 180;
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(config->comparator, pulse_width_us));
}
