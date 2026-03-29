#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"

#include "app_data.h"
#include "i2c_bus.h"
#include "oled_042.h"
#include "bh1750.h"
#include "mpu6050.h"
#include "max3010x.h"

static const char *TAG = "esp32c3_demo";

/* ---------------------------
 * Button
 * --------------------------- */
#define BUTTON_GPIO             GPIO_NUM_1
#define BUTTON_ACTIVE_LEVEL     0
#define BUTTON_DEBOUNCE_MS      30

/* ---------------------------
 * Chart
 * --------------------------- */
#define CHART_HISTORY_LEN       60

typedef struct {
    float values[CHART_HISTORY_LEN];
    int head;
    bool filled;
} chart_buffer_t;

typedef enum {
    SCREEN_ACCEL_X = 0,
    SCREEN_ACCEL_Y,
    SCREEN_ACCEL_Z,
    SCREEN_GYRO_X,
    SCREEN_GYRO_Y,
    SCREEN_GYRO_Z,
    SCREEN_LIGHT,
    SCREEN_HEART,
    SCREEN_COUNT
} screen_mode_t;

/* ---------------------------
 * Shared sensor data
 * --------------------------- */
static sensor_data_t g_sensor_data = {0};
static SemaphoreHandle_t g_sensor_mutex = NULL;

/* ---------------------------
 * Helpers
 * --------------------------- */
static void copy_sensor_data(sensor_data_t *out)
{
    if (!out) {
        return;
    }

    if (xSemaphoreTake(g_sensor_mutex, portMAX_DELAY) == pdTRUE) {
        *out = g_sensor_data;
        xSemaphoreGive(g_sensor_mutex);
    }
}

static void update_sensor_data(const sensor_data_t *in)
{
    if (!in) {
        return;
    }

    if (xSemaphoreTake(g_sensor_mutex, portMAX_DELAY) == pdTRUE) {
        g_sensor_data = *in;
        xSemaphoreGive(g_sensor_mutex);
    }
}

static void button_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

static bool button_pressed_event(void)
{
    static int last_raw = 1;
    static int stable_state = 1;
    static TickType_t last_change_tick = 0;

    int raw = gpio_get_level(BUTTON_GPIO);
    TickType_t now = xTaskGetTickCount();

    if (raw != last_raw) {
        last_raw = raw;
        last_change_tick = now;
    }

    if ((now - last_change_tick) >= pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS)) {
        if (stable_state != raw) {
            stable_state = raw;
            if (stable_state == BUTTON_ACTIVE_LEVEL) {
                return true;
            }
        }
    }

    return false;
}

static void chart_push(chart_buffer_t *buf, float v)
{
    if (!buf) {
        return;
    }

    buf->values[buf->head] = v;
    buf->head = (buf->head + 1) % CHART_HISTORY_LEN;
    if (buf->head == 0) {
        buf->filled = true;
    }
}

static int chart_count(const chart_buffer_t *buf)
{
    return buf->filled ? CHART_HISTORY_LEN : buf->head;
}

static int clamp_i(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void draw_line_safe(oled_042_t *oled, int x0, int y0, int x1, int y1, bool color)
{
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        oled_042_set_pixel(oled, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_chart_frame(oled_042_t *oled, int x, int y, int w, int h)
{
    oled_042_draw_rect(oled, x, y, w, h, true);

    int mid = y + h / 2;
    for (int xx = x + 1; xx < x + w - 1; xx += 2) {
        oled_042_set_pixel(oled, xx, mid, true);
    }
}

static void draw_chart_single(oled_042_t *oled, int x, int y, int w, int h,
                              const chart_buffer_t *buf, float min_v, float max_v)
{
    draw_chart_frame(oled, x, y, w, h);

    int count = chart_count(buf);
    if (count < 2) {
        return;
    }

    float range = max_v - min_v;
    if (fabsf(range) < 0.0001f) {
        range = 1.0f;
        min_v -= 0.5f;
        max_v += 0.5f;
    }

    int start = buf->filled ? buf->head : 0;

    int prev_x = 0;
    int prev_y = 0;
    bool has_prev = false;

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % CHART_HISTORY_LEN;
        float v = buf->values[idx];

        int px = x + 1 + (i * (w - 3)) / (count - 1);
        float norm = (v - min_v) / range;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;

        int py = y + h - 2 - (int)lrintf(norm * (float)(h - 3));
        py = clamp_i(py, y + 1, y + h - 2);

        if (has_prev) {
            draw_line_safe(oled, prev_x, prev_y, px, py, true);
        }

        prev_x = px;
        prev_y = py;
        has_prev = true;
    }
}

static void draw_chart_centered(oled_042_t *oled, int x, int y, int w, int h,
                                const chart_buffer_t *buf, float abs_limit)
{
    if (abs_limit < 0.001f) {
        abs_limit = 1.0f;
    }
    draw_chart_single(oled, x, y, w, h, buf, -abs_limit, abs_limit);
}

static void draw_chart_auto(oled_042_t *oled, int x, int y, int w, int h,
                            const chart_buffer_t *buf)
{
    int count = chart_count(buf);
    if (count <= 0) {
        draw_chart_frame(oled, x, y, w, h);
        return;
    }

    int start = buf->filled ? buf->head : 0;
    float min_v = 0.0f;
    float max_v = 0.0f;
    bool first = true;

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % CHART_HISTORY_LEN;
        float v = buf->values[idx];
        if (first) {
            min_v = max_v = v;
            first = false;
        } else {
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
        }
    }

    if (fabsf(max_v - min_v) < 0.001f) {
        min_v -= 1.0f;
        max_v += 1.0f;
    }

    draw_chart_single(oled, x, y, w, h, buf, min_v, max_v);
}

static void i2c_scan(void)
{
    i2c_port_t port = app_i2c_port();

    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        esp_err_t err = i2c_master_write_to_device(port, addr, NULL, 0, pdMS_TO_TICKS(20));
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Found I2C device at 0x%02X", addr);
        }
    }
}

/* ---------------------------
 * Sensor task
 * --------------------------- */
static void sensor_task(void *arg)
{
    (void)arg;

    bh1750_t bh1750;
    mpu6050_t mpu6050;
    max3010x_t max3010x;

    sensor_data_t local = {0};

    local.bh1750_ok = (bh1750_init(&bh1750, app_i2c_port(), BH1750_I2C_ADDR) == ESP_OK);
    local.mpu6050_ok = (mpu6050_init(&mpu6050, app_i2c_port(), MPU6050_I2C_ADDR) == ESP_OK);
    local.max3010x_ok = (max3010x_init(&max3010x, app_i2c_port(), MAX3010X_I2C_ADDR) == ESP_OK);

    update_sensor_data(&local);

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        sensor_data_t next = {0};
        copy_sensor_data(&next);

        if (next.bh1750_ok) {
            float lux = 0.0f;
            if (bh1750_read_lux(&bh1750, &lux) == ESP_OK) {
                next.lux = lux;
            }
        }

        if (next.mpu6050_ok) {
            mpu6050_data_t mpu = {0};
            if (mpu6050_read(&mpu6050, &mpu) == ESP_OK) {
                next.ax = mpu.ax;
                next.ay = mpu.ay;
                next.az = mpu.az;
                next.gx = mpu.gx;
                next.gy = mpu.gy;
                next.gz = mpu.gz;
                next.temp_c = mpu.temp_c;
            }
        }

        if (next.max3010x_ok) {
            max3010x_data_t hr = {0};
            uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
            esp_err_t err = max3010x_read(&max3010x, &hr, now_ms);
            if (err == ESP_OK) {
                next.hr_red = hr.red;
                next.hr_ir = hr.ir;
                next.bpm = hr.bpm;
                next.finger_present = hr.finger_present;
            }
        }

        update_sensor_data(&next);

        ESP_LOGI(TAG,
                 "BH:%d LUX=%.1f | MPU:%d A(%.2f %.2f %.2f) G(%.1f %.1f %.1f) | MAX:%d IR=%" PRIu32 " RED=%" PRIu32 " BPM=%d F=%d",
                 next.bh1750_ok, next.lux,
                 next.mpu6050_ok, next.ax, next.ay, next.az, next.gx, next.gy, next.gz,
                 next.max3010x_ok, next.hr_ir, next.hr_red, next.bpm, next.finger_present);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}

/* ---------------------------
 * UI
 * --------------------------- */
static void draw_boot_screen(oled_042_t *oled, const sensor_data_t *data)
{
    oled_042_clear(oled);
    oled_042_draw_text(oled, 0, 0, "ESP32-C3 DEMO", true);
    oled_042_draw_text(oled, 0, 10, data->bh1750_ok ? "BH1750: OK" : "BH1750: ERR", true);
    oled_042_draw_text(oled, 0, 20, data->mpu6050_ok ? "MPU6050: OK" : "MPU6050: ERR", true);
    oled_042_draw_text(oled, 0, 30, data->max3010x_ok ? "MAX3010X: OK" : "MAX3010X: ERR", true);
    oled_042_update(oled);
}

static void draw_single_value_screen(oled_042_t *oled,
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
        draw_chart_centered(oled, 0, 16, 72, 24, hist, abs_limit);
    } else {
        draw_chart_auto(oled, 0, 16, 72, 24, hist);
    }

    oled_042_update(oled);
}

void app_main(void)
{
    ESP_ERROR_CHECK(app_i2c_init());

    g_sensor_mutex = xSemaphoreCreateMutex();
    if (g_sensor_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor mutex");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    button_init();
    i2c_scan();

    oled_042_t oled;
    ESP_ERROR_CHECK(oled_042_init(&oled, app_i2c_port(), OLED042_I2C_ADDR));

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);

    vTaskDelay(pdMS_TO_TICKS(800));

    sensor_data_t data = {0};
    copy_sensor_data(&data);
    draw_boot_screen(&oled, &data);
    vTaskDelay(pdMS_TO_TICKS(1500));

    chart_buffer_t hist_ax = {0};
    chart_buffer_t hist_ay = {0};
    chart_buffer_t hist_az = {0};
    chart_buffer_t hist_gx = {0};
    chart_buffer_t hist_gy = {0};
    chart_buffer_t hist_gz = {0};
    chart_buffer_t hist_lux = {0};
    chart_buffer_t hist_bpm = {0};

    screen_mode_t screen = SCREEN_ACCEL_X;
    TickType_t last_hist_sample = 0;

    while (1) {
        copy_sensor_data(&data);

        if (button_pressed_event()) {
            screen = (screen + 1) % SCREEN_COUNT;
            ESP_LOGI(TAG, "Switched screen to %d", (int)screen);
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_hist_sample) >= pdMS_TO_TICKS(100)) {
            chart_push(&hist_ax, data.ax);
            chart_push(&hist_ay, data.ay);
            chart_push(&hist_az, data.az);
            chart_push(&hist_gx, data.gx);
            chart_push(&hist_gy, data.gy);
            chart_push(&hist_gz, data.gz);
            chart_push(&hist_lux, data.lux);
            chart_push(&hist_bpm, data.finger_present ? (float)data.bpm : 0.0f);
            last_hist_sample = now;
        }

        char line[24];

        switch (screen) {
            case SCREEN_ACCEL_X:
                if (!data.mpu6050_ok) {
                    draw_single_value_screen(&oled, "ACCEL X", "MPU ERR", &hist_ax, true, 2.5f);
                } else {
                    snprintf(line, sizeof(line), "% .2f G", data.ax);
                    draw_single_value_screen(&oled, "ACCEL X", line, &hist_ax, true, 2.5f);
                }
                break;

            case SCREEN_ACCEL_Y:
                if (!data.mpu6050_ok) {
                    draw_single_value_screen(&oled, "ACCEL Y", "MPU ERR", &hist_ay, true, 2.5f);
                } else {
                    snprintf(line, sizeof(line), "% .2f G", data.ay);
                    draw_single_value_screen(&oled, "ACCEL Y", line, &hist_ay, true, 2.5f);
                }
                break;

            case SCREEN_ACCEL_Z:
                if (!data.mpu6050_ok) {
                    draw_single_value_screen(&oled, "ACCEL Z", "MPU ERR", &hist_az, true, 2.5f);
                } else {
                    snprintf(line, sizeof(line), "% .2f G", data.az);
                    draw_single_value_screen(&oled, "ACCEL Z", line, &hist_az, true, 2.5f);
                }
                break;

            case SCREEN_GYRO_X:
                if (!data.mpu6050_ok) {
                    draw_single_value_screen(&oled, "GYRO X", "MPU ERR", &hist_gx, true, 250.0f);
                } else {
                    snprintf(line, sizeof(line), "% .1f D", data.gx);
                    draw_single_value_screen(&oled, "GYRO X", line, &hist_gx, true, 250.0f);
                }
                break;

            case SCREEN_GYRO_Y:
                if (!data.mpu6050_ok) {
                    draw_single_value_screen(&oled, "GYRO Y", "MPU ERR", &hist_gy, true, 250.0f);
                } else {
                    snprintf(line, sizeof(line), "% .1f D", data.gy);
                    draw_single_value_screen(&oled, "GYRO Y", line, &hist_gy, true, 250.0f);
                }
                break;

            case SCREEN_GYRO_Z:
                if (!data.mpu6050_ok) {
                    draw_single_value_screen(&oled, "GYRO Z", "MPU ERR", &hist_gz, true, 250.0f);
                } else {
                    snprintf(line, sizeof(line), "% .1f D", data.gz);
                    draw_single_value_screen(&oled, "GYRO Z", line, &hist_gz, true, 250.0f);
                }
                break;

            case SCREEN_LIGHT:
                if (!data.bh1750_ok) {
                    draw_single_value_screen(&oled, "LIGHT", "BH1750 ERR", &hist_lux, false, 0.0f);
                } else {
                    snprintf(line, sizeof(line), "%.0f LX", data.lux);
                    draw_single_value_screen(&oled, "LIGHT", line, &hist_lux, false, 0.0f);
                }
                break;

            case SCREEN_HEART:
                if (!data.max3010x_ok) {
                    draw_single_value_screen(&oled, "HEART", "MAX ERR", &hist_bpm, false, 0.0f);
                } else if (!data.finger_present) {
                    draw_single_value_screen(&oled, "HEART", "NO FINGER", &hist_bpm, false, 0.0f);
                } else {
                    snprintf(line, sizeof(line), "%d BPM", data.bpm);
                    draw_single_value_screen(&oled, "HEART", line, &hist_bpm, false, 0.0f);
                }
                break;

            default:
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
