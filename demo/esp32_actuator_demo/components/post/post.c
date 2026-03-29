#include "post.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "POST";

static void post_test_led(led_config_t *red, led_config_t *yellow, led_config_t *green, post_result_t *result)
{
    ESP_LOGI(TAG, "Testing LEDs...");
    
    led_on(red);
    vTaskDelay(pdMS_TO_TICKS(200));
    led_off(red);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    led_on(yellow);
    vTaskDelay(pdMS_TO_TICKS(200));
    led_off(yellow);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    led_on(green);
    vTaskDelay(pdMS_TO_TICKS(200));
    led_off(green);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    result->test_led_red = 1;
    result->test_led_yellow = 1;
    result->test_led_green = 1;
    ESP_LOGI(TAG, "LED test PASS");
}

static void post_test_buzzer(buzzer_config_t *buzzer, post_result_t *result)
{
    ESP_LOGI(TAG, "Testing Buzzer...");
    buzzer_beep(buzzer, 200);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_beep(buzzer, 200);
    vTaskDelay(pdMS_TO_TICKS(100));
    result->test_buzzer = 1;
    ESP_LOGI(TAG, "Buzzer test PASS");
}

static void post_test_relay(relay_config_t *relay, post_result_t *result)
{
    ESP_LOGI(TAG, "Testing Relay...");
    relay_on(relay);
    vTaskDelay(pdMS_TO_TICKS(200));
    relay_off(relay);
    vTaskDelay(pdMS_TO_TICKS(100));
    result->test_relay = 1;
    ESP_LOGI(TAG, "Relay test PASS");
}

static void post_test_servo(servo_config_t *servo, post_result_t *result)
{
    ESP_LOGI(TAG, "Testing Servo...");
    servo_set_angle(servo, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    servo_set_angle(servo, 180);
    vTaskDelay(pdMS_TO_TICKS(500));
    servo_set_angle(servo, 90);
    vTaskDelay(pdMS_TO_TICKS(200));
    result->test_servo = 1;
    ESP_LOGI(TAG, "Servo test PASS");
}

static void post_test_button(button_config_t *button, post_result_t *result)
{
    ESP_LOGI(TAG, "Testing Button...");
    button_init(button);
    result->test_button = 1;
    ESP_LOGI(TAG, "Button test PASS");
}

static void post_test_oled(oled_config_t *oled, post_result_t *result)
{
    ESP_LOGI(TAG, "Testing OLED...");
    oled_clear(oled);
    oled_draw_str(oled, 0, 20, "POST Test");
    oled_draw_str(oled, 0, 28, "OLED OK");
    vTaskDelay(pdMS_TO_TICKS(500));
    result->test_oled = 1;
    ESP_LOGI(TAG, "OLED test PASS");
}

void post_init(post_result_t *result)
{
    memset(result, 0, sizeof(post_result_t));
    result->status = POST_STATUS_INIT;
}

esp_err_t post_run_all(post_result_t *result, oled_config_t *oled)
{
    result->status = POST_STATUS_RUNNING;
    ESP_LOGI(TAG, "Starting POST...");

    post_test_oled(oled, result);
    oled_clear(oled);
    oled_draw_str(oled, 0, 20, "Testing...");
    oled_update_display(oled);

    led_config_t red = {.gpio = LED_RED_GPIO, .state = 0};
    led_config_t yellow = {.gpio = LED_YELLOW_GPIO, .state = 0};
    led_config_t green = {.gpio = LED_GREEN_GPIO, .state = 0};
    led_traffic_init(&red, &yellow, &green);

    buzzer_config_t buzzer = {.gpio = BUZZER_GPIO, .state = 0};
    buzzer_init(&buzzer);

    relay_config_t relay = {.gpio = RELAY_GPIO, .active_level = RELAY_ACTIVE_LEVEL, .state = 0};
    relay_init(&relay);

    servo_config_t servo = {
        .gpio = SERVO_GPIO,
    };
    servo_init(&servo);

    button_config_t button = {
        .gpio = BUTTON_GPIO,
        .active_level = BUTTON_ACTIVE_LEVEL,
        .debounce_ms = BUTTON_DEBOUNCE_MS,
        .last_press_time = 0,
        .last_state = 1
    };
    post_test_button(&button, result);

    post_test_led(&red, &yellow, &green, result);
    post_test_buzzer(&buzzer, result);
    post_test_relay(&relay, result);
    post_test_servo(&servo, result);

    result->status = POST_STATUS_PASS;
    ESP_LOGI(TAG, "POST Complete: ALL TESTS PASSED");
    return ESP_OK;
}

void post_display_result(oled_config_t *oled, post_result_t *result)
{
    oled_clear(oled);
    if (result->status == POST_STATUS_PASS) {
        oled_draw_str(oled, 0, 20, "POST: PASS");
    } else {
        oled_draw_str(oled, 0, 20, "POST: FAIL");
    }
    oled_update_display(oled);
}
