#ifndef I2C_BUS_H
#define I2C_BUS_H

#include "esp_err.h"
#include "driver/i2c.h"

#include "board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_i2c_init(void);
i2c_port_t app_i2c_port(void);

#ifdef __cplusplus
}
#endif

#endif // I2C_BUS_H
