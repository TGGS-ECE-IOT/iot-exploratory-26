#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "pin_config.h"

typedef struct {
    gpio_num_t sda_gpio;
    gpio_num_t scl_gpio;
    i2c_port_t i2c_port;
    uint8_t address;
    uint8_t buffer[128 * 32 / 8];
} oled_config_t;

void oled_init(oled_config_t *config);
void oled_clear(oled_config_t *config);
void oled_draw_str(oled_config_t *config, uint8_t x, uint8_t y, const char *str);
void oled_draw_str_big(oled_config_t *config, uint8_t x, uint8_t y, const char *str);
void oled_draw_int(oled_config_t *config, uint8_t x, uint8_t y, int value);
void oled_update_display(oled_config_t *config);
void oled_draw_bitmap_full(oled_config_t *config, const uint8_t *bitmap);

#endif
