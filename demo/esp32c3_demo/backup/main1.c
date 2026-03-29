#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <stdbool.h>

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

/* =========================
 * Button configuration
 * ========================= */
#define BUTTON_GPIO             GPIO_NUM_1
#define BUTTON_ACTIVE_LEVEL     0
#define BUTTON_DEBOUNCE_MS      30

/* =========================
 * Chart configuration
 * ========================= */
#define CHART_HISTORY_LEN       46

typedef enum {
    DISPLAY_MODE_ACCEL = 0,
    DISPLAY_MODE_GYRO,
    DISPLAY_MODE_LIGHT,
    DISPLAY_MODE_HEART,
    DISPLAY_MODE_COUNT
} display_mode_t;

typedef struct {
    float values[CHART_HISTORY_LEN];
    int head;
    bool filled;
} chart_buffer_t;

typedef struct {
    float x[CHART_HISTORY_LEN];
    float y[CHART_HISTORY_LEN];
    float z[CHART_HISTORY_LEN];
    int head;
    bool filled;
} chart3_buffer_t;

/* =========================
 * Shared sensor data
 * ========================= */
static sensor_data_t g_sensor_data = {0};
static SemaphoreHandle_t g_sensor_mutex = NULL;

/* =========================
 * Forward declarations
 * ========================= */
static void copy_sensor_data(sensor_data_t *out);
static void update_sensor_data(const sensor_data_t *in);
static void i2c_scan(void);
static void sensor_task(void *arg);

static void button_init(void);
static bool button_pressed_event(void);

static void chart_push(chart_buffer_t *buf, float v);
static void chart3_push(chart3_buffer_t *buf, float xv, float yv, float zv);

static void draw_line_safe(oled_042_t *oled, int x0, int y0, int x1, int y1, bool color);
static void draw_chart_frame(oled_042_t *oled, int x, int y, int w, int h);
static void draw_chart_single(oled_042_t *oled, int x, int y, int w, int h, const chart_buffer_t *buf, float min_v, float max_v);
static void draw_chart_single_auto(oled_042_t *oled, int x, int y, int w, int h, const chart_buffer_t *buf);
static void draw_chart_single_centered(oled_042_t *oled, int x, int y, int w, int h, const chart_buffer_t *buf, float abs_limit);
static void draw_chart_triple_row(
    oled_042_t *oled,
    int x, int y, int w, int h,
    const chart3_buffer_t *buf,
    float abs_limit
);

static void draw_boot_screen(oled_042_t *oled, const sensor_data_t *data);
static void draw_screen_accel(oled_042_t *oled, const sensor_data_t *data, const chart3_buffer_t *hist);
static void draw_screen_gyro(oled_042_t *oled, const sensor_data_t *data, const chart3_buffer_t *hist);
static void draw_screen_light(oled_042_t *oled, const sensor_data_t *data, const chart_buffer_t *hist);
static void draw_screen_heart(oled_042_t *oled, const sensor_data_t *data, const chart_buffer_t *hist);

/* =========================
 * Shared data helpers
 * ========================= */
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

/* =========================
 * I2C scanner
 * ========================= */
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

/* =========================
 * Sensor task
 * ========================= */
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
            } else {
                next.bh1750_ok = false;
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
            } else {
                next.mpu6050_ok = false;
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
            } else if (err != ESP_ERR_NOT_FOUND) {
                next.max3010x_ok = false;
            }
        }

        update_sensor_data(&next);

        ESP_LOGI(TAG,
                 "BH:%d LUX=%.1f | MPU:%d A(%.2f %.2f %.2f) G(%.1f %.1f %.1f) T=%.1f | MAX:%d IR=%" PRIu32 " RED=%" PRIu32 " BPM=%d F=%d",
                 next.bh1750_ok, next.lux,
                 next.mpu6050_ok, next.ax, next.ay, next.az, next.gx, next.gy, next.gz, next.temp_c,
                 next.max3010x_ok, next.hr_ir, next.hr_red, next.bpm, next.finger_present);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}

/* =========================
 * Button handling
 * ========================= */
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

/* =========================
 * Chart buffers
 * ========================= */
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

static void chart3_push(chart3_buffer_t *buf, float xv, float yv, float zv)
{
    if (!buf) {
        return;
    }

    buf->x[buf->head] = xv;
    buf->y[buf->head] = yv;
    buf->z[buf->head] = zv;

    buf->head = (buf->head + 1) % CHART_HISTORY_LEN;
    if (buf->head == 0) {
        buf->filled = true;
    }
}

/* =========================
 * Drawing helpers
 * ========================= */
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

static int clamp_i(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void draw_chart_single(oled_042_t *oled, int x, int y, int w, int h, const chart_buffer_t *buf, float min_v, float max_v)
{
    if (!buf || w < 3 || h < 3) {
        return;
    }

    draw_chart_frame(oled, x, y, w, h);

    int count = buf->filled ? CHART_HISTORY_LEN : buf->head;
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

static void draw_chart_single_auto(oled_042_t *oled, int x, int y, int w, int h, const chart_buffer_t *buf)
{
    int count = buf->filled ? CHART_HISTORY_LEN : buf->head;
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

static void draw_chart_single_centered(oled_042_t *oled, int x, int y, int w, int h, const chart_buffer_t *buf, float abs_limit)
{
    if (abs_limit < 0.001f) {
        abs_limit = 1.0f;
    }
    draw_chart_single(oled, x, y, w, h, buf, -abs_limit, abs_limit);
}

static void draw_chart_triple_row(
    oled_042_t *oled,
    int x, int y, int w, int h,
    const chart3_buffer_t *buf,
    float abs_limit
)
{
    chart_buffer_t bx = {0};
    chart_buffer_t by = {0};
    chart_buffer_t bz = {0};

    bx.head = buf->head;
    bx.filled = buf->filled;
    by.head = buf->head;
    by.filled = buf->filled;
    bz.head = buf->head;
    bz.filled = buf->filled;

    memcpy(bx.values, buf->x, sizeof(bx.values));
    memcpy(by.values, buf->y, sizeof(by.values));
    memcpy(bz.values, buf->z, sizeof(bz.values));

    int row_h = h / 3;
    if (row_h < 6) {
        return;
    }

    draw_chart_single_centered(oled, x, y + 0 * row_h, w, row_h, &bx, abs_limit);
    draw_chart_single_centered(oled, x, y + 1 * row_h, w, row_h, &by, abs_limit);
    draw_chart_single_centered(oled, x, y + 2 * row_h, w, h - 2 * row_h, &bz, abs_limit);
}

/* =========================
 * Screen drawing
 * ========================= */
static void draw_boot_screen(oled_042_t *oled, const sensor_data_t *data)
{
    oled_042_clear(oled);
    oled_042_draw_text(oled, 0, 0, "ESP32-C3 DEMO", true);
    oled_042_draw_text(oled, 0, 10, data->bh1750_ok ? "BH1750: OK" : "BH1750: ERR", true);
    oled_042_draw_text(oled, 0, 20, data->mpu6050_ok ? "MPU6050:OK" : "MPU6050:ER", true);
    oled_042_draw_text(oled, 0, 30, data->max3010x_ok ? "MAX3010X:OK" : "MAX3010X:ER", true);
    oled_042_update(oled);
}

static void draw_screen_accel(oled_042_t *oled, const sensor_data_t *data, const chart3_buffer_t *hist)
{
    char line[24];

    oled_042_clear(oled);
    oled_042_draw_text(oled, 0, 0, "ACCEL", true);

    snprintf(line, sizeof(line), "X% .1f", data->ax);
    oled_042_draw_text(oled, 0, 9, line, true);

    snprintf(line, sizeof(line), "Y% .1f", data->ay);
    oled_042_draw_text(oled, 0, 19, line, true);

    snprintf(line, sizeof(line), "Z% .1f", data->az);
    oled_042_draw_text(oled, 0, 29, line, true);

    draw_chart_triple_row(oled, 28, 8, 44, 32, hist, 2.5f);
    oled_042_update(oled);
}

static void draw_screen_gyro(oled_042_t *oled, const sensor_data_t *data, const chart3_buffer_t *hist)
{
    char line[24];

    oled_042_clear(oled);
    oled_042_draw_text(oled, 0, 0, "GYRO", true);

    snprintf(line, sizeof(line), "X% .0f", data->gx);
    oled_042_draw_text(oled, 0, 9, line, true);

    snprintf(line, sizeof(line), "Y% .0f", data->gy);
    oled_042_draw_text(oled, 0, 19, line, true);

    snprintf(line, sizeof(line), "Z% .0f", data->gz);
    oled_042_draw_text(oled, 0, 29, line, true);

    draw_chart_triple_row(oled, 28, 8, 44, 32, hist, 250.0f);
    oled_042_update(oled);
}

static void draw_screen_light(oled_042_t *oled, const sensor_data_t *data, const chart_buffer_t *hist)
{
    char line[24];

    oled_042_clear(oled);
    oled_042_draw_text(oled, 0, 0, "LIGHT", true);

    snprintf(line, sizeof(line), "%4.0f LX", data->lux);
    oled_042_draw_text(oled, 0, 8, line, true);

    draw_chart_single_auto(oled, 0, 16, 72, 24, hist);
    oled_042_update(oled);
}

static void draw_screen_heart(oled_042_t *oled, const sensor_data_t *data, const chart_buffer_t *hist)
{
    char line[24];

    oled_042_clear(oled);
    oled_042_draw_text(oled, 0, 0, "HEART", true);

    if (data->finger_present) {
        snprintf(line, sizeof(line), "%3d BPM", data->bpm);
    } else {
        snprintf(line, sizeof(line), "NO FINGER");
    }
    oled_042_draw_text(oled, 0, 8, line, true);

    draw_chart_single_auto(oled, 0, 16, 72, 24, hist);
    oled_042_update(oled);
}

/* =========================
 * Main
 * ========================= */
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

    chart3_buffer_t accel_hist = {0};
    chart3_buffer_t gyro_hist = {0};
    chart_buffer_t light_hist = {0};
    chart_buffer_t heart_hist = {0};

    display_mode_t mode = DISPLAY_MODE_ACCEL;
    TickType_t last_hist_sample = 0;

    while (1) {
        copy_sensor_data(&data);

        if (button_pressed_event()) {
            mode = (display_mode_t)((mode + 1) % DISPLAY_MODE_COUNT);
            ESP_LOGI(TAG, "Switched mode to %d", (int)mode);
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_hist_sample) >= pdMS_TO_TICKS(100)) {
            chart3_push(&accel_hist, data.ax, data.ay, data.az);
            chart3_push(&gyro_hist, data.gx, data.gy, data.gz);
            chart_push(&light_hist, data.lux);

            if (data.finger_present) {
                chart_push(&heart_hist, (float)data.bpm);
            } else {
                chart_push(&heart_hist, 0.0f);
            }

            last_hist_sample = now;
        }

        switch (mode) {
            case DISPLAY_MODE_ACCEL:
                draw_screen_accel(&oled, &data, &accel_hist);
                break;

            case DISPLAY_MODE_GYRO:
                draw_screen_gyro(&oled, &data, &gyro_hist);
                break;

            case DISPLAY_MODE_LIGHT:
                draw_screen_light(&oled, &data, &light_hist);
                break;

            case DISPLAY_MODE_HEART:
                draw_screen_heart(&oled, &data, &heart_hist);
                break;

            default:
                draw_screen_accel(&oled, &data, &accel_hist);
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}