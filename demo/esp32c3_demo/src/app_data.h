#ifndef APP_DATA_H
#define APP_DATA_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool bh1750_ok;
    bool mpu6050_ok;
    bool max30102_ok;

    float lux;

    float ax;
    float ay;
    float az;

    float gx;
    float gy;
    float gz;

    float temp_c;

    uint32_t hr_red;
    uint32_t hr_ir;
    int bpm;
    bool finger_present;
} sensor_data_t;

#endif /* APP_DATA_H */
