#include "mpu6050.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static esp_err_t mpu6050_write_reg(mpu6050_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(dev->port, dev->addr, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t mpu6050_read_regs(mpu6050_t *dev, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(dev->port, dev->addr, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

esp_err_t mpu6050_init(mpu6050_t *dev, i2c_port_t port, uint8_t addr)
{
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }

    dev->port = port;
    dev->addr = addr;

    esp_err_t err = mpu6050_write_reg(dev, 0x6B, 0x00);
    if (err != ESP_OK) return err;

    err = mpu6050_write_reg(dev, 0x1B, 0x00);
    if (err != ESP_OK) return err;

    err = mpu6050_write_reg(dev, 0x1C, 0x00);
    if (err != ESP_OK) return err;

    err = mpu6050_write_reg(dev, 0x1A, 0x03);
    if (err != ESP_OK) return err;

    return ESP_OK;
}

esp_err_t mpu6050_read(mpu6050_t *dev, mpu6050_data_t *out)
{
    if (!dev || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[14];
    esp_err_t err = mpu6050_read_regs(dev, 0x3B, data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }

    int16_t ax = (int16_t)((data[0] << 8) | data[1]);
    int16_t ay = (int16_t)((data[2] << 8) | data[3]);
    int16_t az = (int16_t)((data[4] << 8) | data[5]);
    int16_t temp = (int16_t)((data[6] << 8) | data[7]);
    int16_t gx = (int16_t)((data[8] << 8) | data[9]);
    int16_t gy = (int16_t)((data[10] << 8) | data[11]);
    int16_t gz = (int16_t)((data[12] << 8) | data[13]);

    out->ax = (float)ax / 16384.0f;
    out->ay = (float)ay / 16384.0f;
    out->az = (float)az / 16384.0f;

    out->gx = (float)gx / 131.0f;
    out->gy = (float)gy / 131.0f;
    out->gz = (float)gz / 131.0f;

    out->temp_c = ((float)temp / 340.0f) + 36.53f;

    return ESP_OK;
}
