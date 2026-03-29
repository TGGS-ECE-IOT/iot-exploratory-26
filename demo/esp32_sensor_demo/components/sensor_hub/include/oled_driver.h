#ifndef OLED_DRIVER_H
#define OLED_DRIVER_H

#include <stdint.h>

void oled_driver_init(void);
void oled_clear_buffer(void);
void oled_draw_text(int x, int page, const char *text);
void oled_draw_text_centered(int y, const char *text);
void oled_draw_text_right(int y, const char *text);
void oled_draw_xbm(int x, int y, int width, int height, const uint8_t *bitmap);
void oled_draw_line(int x0, int y0, int x1, int y1);
void oled_draw_circle(int x, int y, int radius);
void oled_set_font_small(void);
void oled_set_font_large(void);
void oled_refresh(void);

#endif
