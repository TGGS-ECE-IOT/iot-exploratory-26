#include "max30102.h"

#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX30102_REG_INT_STATUS_1       0x00
#define MAX30102_REG_INT_STATUS_2       0x01
#define MAX30102_REG_INT_ENABLE_1       0x02
#define MAX30102_REG_INT_ENABLE_2       0x03
#define MAX30102_REG_FIFO_WR_PTR        0x04
#define MAX30102_REG_OVF_COUNTER        0x05
#define MAX30102_REG_FIFO_RD_PTR        0x06
#define MAX30102_REG_FIFO_DATA          0x07
#define MAX30102_REG_FIFO_CONFIG        0x08
#define MAX30102_REG_MODE_CONFIG        0x09
#define MAX30102_REG_SPO2_CONFIG        0x0A
#define MAX30102_REG_LED1_PA            0x0C
#define MAX30102_REG_LED2_PA            0x0D
#define MAX30102_REG_PART_ID            0xFF

#define MAX30102_PART_ID_MAX30102       0x15

#define MAX30102_MODE_RESET             0x40
#define MAX30102_MODE_SPO2              0x03

#define MAX30102_FINGER_THRESHOLD       15000U

static esp_err_t max30102_write_reg(max30102_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(dev->port, dev->addr, buf, sizeof(buf), pdMS_TO_TICKS(100));
}

static esp_err_t max30102_read_regs(max30102_t *dev, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(dev->port, dev->addr, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

static esp_err_t max30102_clear_fifo(max30102_t *dev)
{
    esp_err_t err;

    err = max30102_write_reg(dev, MAX30102_REG_FIFO_WR_PTR, 0x00);
    if (err != ESP_OK) return err;

    err = max30102_write_reg(dev, MAX30102_REG_OVF_COUNTER, 0x00);
    if (err != ESP_OK) return err;

    err = max30102_write_reg(dev, MAX30102_REG_FIFO_RD_PTR, 0x00);
    if (err != ESP_OK) return err;

    return ESP_OK;
}

static esp_err_t max30102_fifo_has_sample(max30102_t *dev, bool *has_sample)
{
    if (!dev || !has_sample) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t wr = 0;
    uint8_t rd = 0;
    esp_err_t err;

    err = max30102_read_regs(dev, MAX30102_REG_FIFO_WR_PTR, &wr, 1);
    if (err != ESP_OK) return err;

    err = max30102_read_regs(dev, MAX30102_REG_FIFO_RD_PTR, &rd, 1);
    if (err != ESP_OK) return err;

    *has_sample = (wr != rd);
    return ESP_OK;
}

static esp_err_t max30102_read_fifo_sample(max30102_t *dev, uint32_t *red, uint32_t *ir)
{
    if (!dev || !red || !ir) {
        return ESP_ERR_INVALID_ARG;
    }

    bool has_sample = false;
    esp_err_t err = max30102_fifo_has_sample(dev, &has_sample);
    if (err != ESP_OK) {
        return err;
    }

    if (!has_sample) {
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t data[6] = {0};
    err = max30102_read_regs(dev, MAX30102_REG_FIFO_DATA, data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }

    *red = ((((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2]) & 0x03FFFFU);
    *ir  = ((((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | data[5]) & 0x03FFFFU);

    return ESP_OK;
}

esp_err_t max30102_init(max30102_t *dev, i2c_port_t port, uint8_t addr)
{
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));
    dev->port = port;
    dev->addr = addr;

    uint8_t part_id = 0;
    esp_err_t err = max30102_read_regs(dev, MAX30102_REG_PART_ID, &part_id, 1);
    if (err != ESP_OK) {
        return err;
    }

    if (part_id != MAX30102_PART_ID_MAX30102) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    err = max30102_write_reg(dev, MAX30102_REG_MODE_CONFIG, MAX30102_MODE_RESET);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(100));

    err = max30102_write_reg(dev, MAX30102_REG_INT_ENABLE_1, 0x00);
    if (err != ESP_OK) return err;

    err = max30102_write_reg(dev, MAX30102_REG_INT_ENABLE_2, 0x00);
    if (err != ESP_OK) return err;

    uint8_t dummy[2];
    err = max30102_read_regs(dev, MAX30102_REG_INT_STATUS_1, dummy, sizeof(dummy));
    if (err != ESP_OK) return err;

    err = max30102_clear_fifo(dev);
    if (err != ESP_OK) return err;

    err = max30102_write_reg(dev, MAX30102_REG_FIFO_CONFIG, 0x50);
    if (err != ESP_OK) return err;

    err = max30102_write_reg(dev, MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2);
    if (err != ESP_OK) return err;

    err = max30102_write_reg(dev, MAX30102_REG_SPO2_CONFIG, 0x27);
    if (err != ESP_OK) return err;

    err = max30102_write_reg(dev, MAX30102_REG_LED1_PA, 0x24);
    if (err != ESP_OK) return err;

    err = max30102_write_reg(dev, MAX30102_REG_LED2_PA, 0x24);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

esp_err_t max30102_read(max30102_t *dev, max30102_data_t *out, uint32_t now_ms)
{
    if (!dev || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t red = 0;
    uint32_t ir = 0;

    esp_err_t err = ESP_ERR_NOT_FOUND;
    for (int i = 0; i < 5; i++) {
        err = max30102_read_fifo_sample(dev, &red, &ir);
        if (err == ESP_OK) {
            break;
        }
        if (err != ESP_ERR_NOT_FOUND) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (err != ESP_OK) {
        return err;
    }

    out->red = red;
    out->ir = ir;
    out->finger_present = (ir >= MAX30102_FINGER_THRESHOLD);

    if (!out->finger_present) {
        dev->ir_dc = 0.0f;
        dev->last_above = false;
        dev->bpm = 0;
        out->bpm = 0;
        return ESP_OK;
    }

    if (dev->ir_dc == 0.0f) {
        dev->ir_dc = (float)ir;
    }

    dev->ir_dc = 0.90f * dev->ir_dc + 0.10f * (float)ir;
    float ac = (float)ir - dev->ir_dc;

    float threshold = dev->ir_dc * 0.02f;
    if (threshold < 1000.0f) {
        threshold = 1000.0f;
    }

    bool above = (ac > threshold);

    if (above && !dev->last_above) {
        uint32_t dt = now_ms - dev->last_beat_ms;
        if (dt > 300U && dt < 2000U) {
            dev->bpm = (int)(60000.0f / (float)dt);
        }
        dev->last_beat_ms = now_ms;
    }

    dev->last_above = above;
    out->bpm = dev->bpm;

    return ESP_OK;
}