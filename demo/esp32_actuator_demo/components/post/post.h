#ifndef POST_H
#define POST_H

#include "esp_err.h"
#include "led.h"
#include "buzzer.h"
#include "relay.h"
#include "servo.h"
#include "button.h"
#include "oled.h"

typedef enum {
    POST_STATUS_INIT,
    POST_STATUS_RUNNING,
    POST_STATUS_PASS,
    POST_STATUS_FAIL
} post_status_t;

typedef struct {
    post_status_t status;
    uint8_t test_led_red;
    uint8_t test_led_yellow;
    uint8_t test_led_green;
    uint8_t test_buzzer;
    uint8_t test_relay;
    uint8_t test_servo;
    uint8_t test_button;
    uint8_t test_oled;
} post_result_t;

void post_init(post_result_t *result);
esp_err_t post_run_all(post_result_t *result, oled_config_t *oled);
void post_display_result(oled_config_t *oled, post_result_t *result);

#endif
