#ifndef BH1750_H
#define BH1750_H

#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t port;
    uint8_t addr;
} bh1750_t;

esp_err_t bh1750_init(bh1750_t *dev, i2c_port_t port, uint8_t addr);
esp_err_t bh1750_read_lux(bh1750_t *dev, float *lux);

#ifdef __cplusplus
}
#endif

#endif
