#ifndef OLED_042_H
#define OLED_042_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c.h"
#include "esp_err.h"

#include "board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OLED042_WIDTH     BOARD_OLED_WIDTH
#define OLED042_HEIGHT    BOARD_OLED_HEIGHT
#define OLED042_I2C_ADDR  BOARD_OLED_I2C_ADDR

typedef struct {
    i2c_port_t port;
    uint8_t addr;
    uint8_t buffer[OLED042_WIDTH * OLED042_HEIGHT / 8];
} oled_042_t;

esp_err_t oled_042_init(oled_042_t *oled, i2c_port_t port, uint8_t addr);
esp_err_t oled_042_clear(oled_042_t *oled);
esp_err_t oled_042_update(oled_042_t *oled);
void oled_042_set_pixel(oled_042_t *oled, int x, int y, bool color);
void oled_042_draw_hline(oled_042_t *oled, int x, int y, int w, bool color);
void oled_042_draw_vline(oled_042_t *oled, int x, int y, int h, bool color);
void oled_042_draw_rect(oled_042_t *oled, int x, int y, int w, int h, bool color);
void oled_042_fill_rect(oled_042_t *oled, int x, int y, int w, int h, bool color);
void oled_042_draw_char(oled_042_t *oled, int x, int y, char c, bool color);
void oled_042_draw_text(oled_042_t *oled, int x, int y, const char *text, bool color);

void oled_042_draw_char_2x(oled_042_t *oled, int x, int y, char c, bool color);
void oled_042_draw_text_2x(oled_042_t *oled, int x, int y, const char *text, bool color);


#ifdef __cplusplus
}
#endif

#endif
