#include "bh1750.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BH1750_CMD_CONT_H_RES_MODE  0x10
#define BH1750_CMD_POWER_ON         0x01
#define BH1750_CMD_RESET            0x07

esp_err_t bh1750_init(bh1750_t *dev, i2c_port_t port, uint8_t addr)
{
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }

    dev->port = port;
    dev->addr = addr;

    uint8_t cmd = BH1750_CMD_POWER_ON;
    esp_err_t err = i2c_master_write_to_device(dev->port, dev->addr, &cmd, 1, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        return err;
    }

    cmd = BH1750_CMD_RESET;
    err = i2c_master_write_to_device(dev->port, dev->addr, &cmd, 1, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        return err;
    }

    cmd = BH1750_CMD_CONT_H_RES_MODE;
    return i2c_master_write_to_device(dev->port, dev->addr, &cmd, 1, pdMS_TO_TICKS(100));
}

esp_err_t bh1750_read_lux(bh1750_t *dev, float *lux)
{
    if (!dev || !lux) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[2] = {0};
    esp_err_t err = i2c_master_read_from_device(dev->port, dev->addr, data, 2, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        return err;
    }

    uint16_t raw = ((uint16_t)data[0] << 8) | data[1];
    *lux = (float)raw / 1.2f;
    return ESP_OK;
}
