#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "driver/gpio.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_I2C_PORT           I2C_NUM_0
#define BOARD_I2C_SDA            GPIO_NUM_5
#define BOARD_I2C_SCL            GPIO_NUM_6
#define BOARD_I2C_FREQ_HZ        400000

#define BOARD_BUTTON_GPIO        GPIO_NUM_1
#define BOARD_BUTTON_ACTIVE      0
#define BOARD_BUTTON_DEBOUNCE_MS 25

#define BOARD_BH1750_I2C_ADDR    0x23
#define BOARD_MPU6050_I2C_ADDR   0x68
#define BOARD_MAX30102_I2C_ADDR  0x57
#define BOARD_OLED_I2C_ADDR      0x3C
#define BOARD_OLED_WIDTH         72
#define BOARD_OLED_HEIGHT        40

#ifdef __cplusplus
}
#endif

#endif // BOARD_CONFIG_H
