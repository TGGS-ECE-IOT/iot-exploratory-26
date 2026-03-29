#include "ui_task.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "board_config.h"
#include "i2c_bus.h"
#include "oled_042.h"
#include "sensor_task.h"
#include "chart.h"
#include "ece_logo_72x40.h"

typedef enum {
    SCREEN_LIGHT = 0,
    SCREEN_ACCEL_X,
    SCREEN_ACCEL_Y,
    SCREEN_ACCEL_Z,
    SCREEN_GYRO_ROLL,
    SCREEN_GYRO_PITCH,
    SCREEN_GYRO_YAW,
    SCREEN_HEART,
    SCREEN_COUNT
} ui_screen_t;

static const char *TAG = "ui_task";
static bool g_started = false;

static oled_042_t g_oled;
static chart_buffer_t g_hist_ax;
static chart_buffer_t g_hist_ay;
static chart_buffer_t g_hist_az;
static chart_buffer_t g_hist_gx;
static chart_buffer_t g_hist_gy;
static chart_buffer_t g_hist_gz;
static chart_buffer_t g_hist_lux;
static chart_buffer_t g_hist_bpm;

static void draw_xbm(oled_042_t *oled, int x0, int y0, int w, int h, const uint8_t *bits)
{
    if (!oled || !bits) {
        return;
    }

    int bytes_per_row = (w + 7) / 8;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t b = bits[y * bytes_per_row + (x / 8)];
            if (b & (uint8_t)(1U << (x & 7))) {
                oled_042_set_pixel(oled, x0 + x, y0 + y, true);
            }
        }
    }
}

static void button_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOARD_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

static bool button_pressed_event(void)
{
    static int stable = 1;
    static int last_raw = 1;
    static TickType_t last_change = 0;
    static bool pressed_flag = false;

    int raw = gpio_get_level(BOARD_BUTTON_GPIO);
    TickType_t now = xTaskGetTickCount();

    if (raw != last_raw) {
        last_raw = raw;
        last_change = now;
    }

    if ((now - last_change) < pdMS_TO_TICKS(BOARD_BUTTON_DEBOUNCE_MS)) {
        return false;
    }

    if (stable != raw) {
        stable = raw;

        if (stable == BOARD_BUTTON_ACTIVE) {
            pressed_flag = true;
        } else {
            if (pressed_flag) {
                pressed_flag = false;
                return true;
            }
        }
    }

    return false;
}

static void draw_splash_logo(oled_042_t *oled)
{
    oled_042_clear(oled);
    draw_xbm(oled, 0, 0, ECE_LOGO_WIDTH, ECE_LOGO_HEIGHT, ece_logo_bits);
    oled_042_update(oled);
}

static void draw_splash_text(oled_042_t *oled)
{
    oled_042_clear(oled);
    oled_042_draw_text_2x(oled, 12, 2, "TGGS", true);
    oled_042_draw_text_2x(oled, 0, 20, "KMUTNB", true);
    oled_042_update(oled);
}

static void draw_boot_screen(oled_042_t *oled)
{
    oled_042_clear(oled);
    oled_042_draw_text(oled, 0, 0, "ESP32-C3 DEMO", true);
    oled_042_draw_text(oled, 0, 10, "BTN: NEXT", true);
    oled_042_draw_text(oled, 0, 20, "I2C GPIO5/6", true);
    oled_042_draw_text(oled, 0, 30, "SENSORS OK", true);
    oled_042_update(oled);
}

static void draw_wait_screen(oled_042_t *oled)
{
    oled_042_clear(oled);
    oled_042_draw_text(oled, 0, 0, "LOADING", true);
    oled_042_draw_text(oled, 0, 12, "SENSOR DATA", true);
    oled_042_draw_text(oled, 0, 24, "PLEASE WAIT", true);
    oled_042_update(oled);
}

static void draw_scalar_screen(oled_042_t *oled,
                               const char *title,
                               const char *value_text,
                               const chart_buffer_t *hist,
                               bool centered,
                               float abs_limit)
{
    oled_042_clear(oled);
    oled_042_draw_text(oled, 0, 0, title, true);
    oled_042_draw_text(oled, 0, 8, value_text, true);

    if (centered) {
        chart_draw_centered(oled, 0, 16, 72, 24, hist, abs_limit);
    } else {
        chart_draw_auto(oled, 0, 16, 72, 24, hist);
    }

    oled_042_update(oled);
}

static void ui_task_fn(void *arg)
{
    (void)arg;

    if (oled_042_init(&g_oled, app_i2c_port(), BOARD_OLED_I2C_ADDR) != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed");
        vTaskDelete(NULL);
        return;
    }

    button_init();

    chart_init(&g_hist_ax);
    chart_init(&g_hist_ay);
    chart_init(&g_hist_az);
    chart_init(&g_hist_gx);
    chart_init(&g_hist_gy);
    chart_init(&g_hist_gz);
    chart_init(&g_hist_lux);
    chart_init(&g_hist_bpm);

    draw_splash_logo(&g_oled);
    vTaskDelay(pdMS_TO_TICKS(1600));

    draw_splash_text(&g_oled);
    vTaskDelay(pdMS_TO_TICKS(1600));

    draw_boot_screen(&g_oled);
    vTaskDelay(pdMS_TO_TICKS(1200));

    ui_screen_t screen = SCREEN_LIGHT;
    TickType_t last_hist_sample = 0;
    int hist_samples = 0;

    while (1) {
        sensor_data_t data = {0};
        sensor_data_copy(&data);

        TickType_t now = xTaskGetTickCount();
        if ((now - last_hist_sample) >= pdMS_TO_TICKS(100)) {
            chart_push(&g_hist_ax, data.ax);
            chart_push(&g_hist_ay, data.ay);
            chart_push(&g_hist_az, data.az);
            chart_push(&g_hist_gx, data.gx);
            chart_push(&g_hist_gy, data.gy);
            chart_push(&g_hist_gz, data.gz);
            chart_push(&g_hist_lux, data.lux);
            chart_push(&g_hist_bpm, data.finger_present ? (float)data.bpm : 0.0f);
            last_hist_sample = now;

            if (hist_samples < CHART_HISTORY_LEN) {
                hist_samples++;
            }
        }

        if (button_pressed_event()) {
            screen = (ui_screen_t)((screen + 1) % SCREEN_COUNT);
            ESP_LOGI(TAG, "screen=%d", (int)screen);
        }

        if (hist_samples < 8) {
            draw_wait_screen(&g_oled);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        char line[24];

        switch (screen) {
            case SCREEN_LIGHT:
                if (!data.bh1750_ok) {
                    draw_scalar_screen(&g_oled, "LIGHT", "BH ERR", &g_hist_lux, false, 0.0f);
                } else {
                    snprintf(line, sizeof(line), "%.0f LX", data.lux);
                    draw_scalar_screen(&g_oled, "LIGHT", line, &g_hist_lux, false, 0.0f);
                }
                break;

            case SCREEN_ACCEL_X:
                if (!data.mpu6050_ok) {
                    draw_scalar_screen(&g_oled, "ACCEL X", "MPU ERR", &g_hist_ax, true, 2.5f);
                } else {
                    snprintf(line, sizeof(line), "% .2f G", data.ax);
                    draw_scalar_screen(&g_oled, "ACCEL X", line, &g_hist_ax, true, 2.5f);
                }
                break;

            case SCREEN_ACCEL_Y:
                if (!data.mpu6050_ok) {
                    draw_scalar_screen(&g_oled, "ACCEL Y", "MPU ERR", &g_hist_ay, true, 2.5f);
                } else {
                    snprintf(line, sizeof(line), "% .2f G", data.ay);
                    draw_scalar_screen(&g_oled, "ACCEL Y", line, &g_hist_ay, true, 2.5f);
                }
                break;

            case SCREEN_ACCEL_Z:
                if (!data.mpu6050_ok) {
                    draw_scalar_screen(&g_oled, "ACCEL Z", "MPU ERR", &g_hist_az, true, 2.5f);
                } else {
                    snprintf(line, sizeof(line), "% .2f G", data.az);
                    draw_scalar_screen(&g_oled, "ACCEL Z", line, &g_hist_az, true, 2.5f);
                }
                break;

            case SCREEN_GYRO_ROLL:
                if (!data.mpu6050_ok) {
                    draw_scalar_screen(&g_oled, "GYRO ROLL", "MPU ERR", &g_hist_gx, true, 250.0f);
                } else {
                    snprintf(line, sizeof(line), "% .1f D", data.gx);
                    draw_scalar_screen(&g_oled, "GYRO ROLL", line, &g_hist_gx, true, 250.0f);
                }
                break;

            case SCREEN_GYRO_PITCH:
                if (!data.mpu6050_ok) {
                    draw_scalar_screen(&g_oled, "GYRO PITCH", "MPU ERR", &g_hist_gy, true, 250.0f);
                } else {
                    snprintf(line, sizeof(line), "% .1f D", data.gy);
                    draw_scalar_screen(&g_oled, "GYRO PITCH", line, &g_hist_gy, true, 250.0f);
                }
                break;

            case SCREEN_GYRO_YAW:
                if (!data.mpu6050_ok) {
                    draw_scalar_screen(&g_oled, "GYRO YAW", "MPU ERR", &g_hist_gz, true, 250.0f);
                } else {
                    snprintf(line, sizeof(line), "% .1f D", data.gz);
                    draw_scalar_screen(&g_oled, "GYRO YAW", line, &g_hist_gz, true, 250.0f);
                }
                break;

            case SCREEN_HEART:
                if (!data.max30102_ok) {
                    draw_scalar_screen(&g_oled, "HEART", "MAX ERR", &g_hist_bpm, false, 0.0f);
                } else if (!data.finger_present) {
                    draw_scalar_screen(&g_oled, "HEART", "NO FINGER", &g_hist_bpm, false, 0.0f);
                } else {
                    snprintf(line, sizeof(line), "%d BPM", data.bpm);
                    draw_scalar_screen(&g_oled, "HEART", line, &g_hist_bpm, false, 0.0f);
                }
                break;

            default:
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t ui_task_start(void)
{
    if (g_started) {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(ui_task_fn, "ui_task", 8192, NULL, 4, NULL);
    if (ok != pdPASS) {
        return ESP_FAIL;
    }

    g_started = true;
    return ESP_OK;
}
