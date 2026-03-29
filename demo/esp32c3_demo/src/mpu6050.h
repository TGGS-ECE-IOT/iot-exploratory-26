#ifndef MPU6050_H
#define MPU6050_H

#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t port;
    uint8_t addr;
} mpu6050_t;

typedef struct {
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
    float temp_c;
} mpu6050_data_t;

esp_err_t mpu6050_init(mpu6050_t *dev, i2c_port_t port, uint8_t addr);
esp_err_t mpu6050_read(mpu6050_t *dev, mpu6050_data_t *out);

#ifdef __cplusplus
}
#endif

#endif
