#include "sensor_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "board_config.h"
#include "i2c_bus.h"
#include "bh1750.h"
#include "mpu6050.h"
#include "max30102.h"

static const char *TAG = "sensor_task";

static sensor_data_t g_sensor_data = {0};
static SemaphoreHandle_t g_sensor_mutex = NULL;
static bool g_started = false;

static void sensor_data_update(const sensor_data_t *in)
{
    if (!in || !g_sensor_mutex) {
        return;
    }

    if (xSemaphoreTake(g_sensor_mutex, portMAX_DELAY) == pdTRUE) {
        g_sensor_data = *in;
        xSemaphoreGive(g_sensor_mutex);
    }
}

bool sensor_data_copy(sensor_data_t *out)
{
    if (!out || !g_sensor_mutex) {
        return false;
    }

    if (xSemaphoreTake(g_sensor_mutex, portMAX_DELAY) == pdTRUE) {
        *out = g_sensor_data;
        xSemaphoreGive(g_sensor_mutex);
        return true;
    }

    return false;
}

static void sensor_task_fn(void *arg)
{
    (void)arg;

    bh1750_t bh1750;
    mpu6050_t mpu6050;
    max30102_t max30102;

    sensor_data_t local = {0};

    local.bh1750_ok = (bh1750_init(&bh1750, app_i2c_port(), BOARD_BH1750_I2C_ADDR) == ESP_OK);
    local.mpu6050_ok = (mpu6050_init(&mpu6050, app_i2c_port(), BOARD_MPU6050_I2C_ADDR) == ESP_OK);
    local.max30102_ok = (max30102_init(&max30102, app_i2c_port(), BOARD_MAX30102_I2C_ADDR) == ESP_OK);

    sensor_data_update(&local);

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        sensor_data_t next = {0};
        sensor_data_copy(&next);

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

        if (next.max30102_ok) {
            max30102_data_t hr = {0};
            uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
            if (max30102_read(&max30102, &hr, now_ms) == ESP_OK) {
                next.hr_red = hr.red;
                next.hr_ir = hr.ir;
                next.bpm = hr.bpm;
                next.finger_present = hr.finger_present;
            }
        }

        sensor_data_update(&next);

        ESP_LOGI(TAG,
                 "LUX=%.1f | AX=%.2f AY=%.2f AZ=%.2f | GX=%.1f GY=%.1f GZ=%.1f | IR=%" PRIu32 " RED=%" PRIu32 " BPM=%d F=%d",
                 next.lux, next.ax, next.ay, next.az,
                 next.gx, next.gy, next.gz,
                 next.hr_ir, next.hr_red, next.bpm, next.finger_present);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(80));
    }
}

esp_err_t sensor_task_start(void)
{
    if (g_started) {
        return ESP_OK;
    }

    g_sensor_mutex = xSemaphoreCreateMutex();
    if (!g_sensor_mutex) {
        return ESP_FAIL;
    }

    BaseType_t ok = xTaskCreate(sensor_task_fn, "sensor_task", 6144, NULL, 5, NULL);
    if (ok != pdPASS) {
        return ESP_FAIL;
    }

    g_started = true;
    return ESP_OK;
}
