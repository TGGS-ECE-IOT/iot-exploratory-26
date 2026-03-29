#include "oled_042.h"

#include <ctype.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define OLED_CMD_MODE  0x00
#define OLED_DATA_MODE 0x40
#define OLED_X_OFFSET  28
#define OLED_PAGES     (OLED042_HEIGHT / 8)

static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00},
    {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00},
    {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08},
    {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E},
    {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E}, {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43}
};

static const uint8_t *oled_042_get_glyph(char c)
{
    if (c >= 'a' && c <= 'z') {
        c = (char)toupper((unsigned char)c);
    }

    if (c < 32 || c > 90) {
        c = '?';
    }

    return font5x7[c - 32];
}

static esp_err_t oled_write_cmd(oled_042_t *oled, uint8_t cmd)
{
    uint8_t buf[2] = {OLED_CMD_MODE, cmd};
    return i2c_master_write_to_device(oled->port, oled->addr, buf, sizeof(buf), pdMS_TO_TICKS(100));
}

static esp_err_t oled_write_data(oled_042_t *oled, const uint8_t *data, size_t len)
{
    uint8_t tmp[17];
    tmp[0] = OLED_DATA_MODE;

    while (len > 0) {
        size_t chunk = (len > 16) ? 16 : len;
        memcpy(&tmp[1], data, chunk);
        esp_err_t err = i2c_master_write_to_device(oled->port, oled->addr, tmp, chunk + 1, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            return err;
        }
        data += chunk;
        len -= chunk;
    }

    return ESP_OK;
}

esp_err_t oled_042_clear(oled_042_t *oled)
{
    if (!oled) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(oled->buffer, 0, sizeof(oled->buffer));
    return ESP_OK;
}

esp_err_t oled_042_init(oled_042_t *oled, i2c_port_t port, uint8_t addr)
{
    if (!oled) {
        return ESP_ERR_INVALID_ARG;
    }

    oled->port = port;
    oled->addr = addr;
    oled_042_clear(oled);

    const uint8_t init_seq[] = {
        0xAE,
        0x20, 0x00,
        0xB0,
        0xC8,
        0x00,
        0x10,
        0x40,
        0x81, 0x7F,
        0xA1,
        0xA6,
        0xA8, 0x27,
        0xA4,
        0xD3, 0x00,
        0xD5, 0x80,
        0xD9, 0xF1,
        0xDA, 0x12,
        0xDB, 0x40,
        0x8D, 0x14,
        0xAF
    };

    for (size_t i = 0; i < sizeof(init_seq); i++) {
        esp_err_t err = oled_write_cmd(oled, init_seq[i]);
        if (err != ESP_OK) {
            return err;
        }
    }

    return oled_042_update(oled);
}

esp_err_t oled_042_update(oled_042_t *oled)
{
    if (!oled) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int page = 0; page < OLED_PAGES; page++) {
        esp_err_t err = oled_write_cmd(oled, 0xB0 + page);
        if (err != ESP_OK) return err;

        err = oled_write_cmd(oled, 0x00 + (OLED_X_OFFSET & 0x0F));
        if (err != ESP_OK) return err;

        err = oled_write_cmd(oled, 0x10 + ((OLED_X_OFFSET >> 4) & 0x0F));
        if (err != ESP_OK) return err;

        err = oled_write_data(oled, &oled->buffer[page * OLED042_WIDTH], OLED042_WIDTH);
        if (err != ESP_OK) return err;
    }

    return ESP_OK;
}

void oled_042_set_pixel(oled_042_t *oled, int x, int y, bool color)
{
    if (!oled) return;
    if (x < 0 || x >= OLED042_WIDTH || y < 0 || y >= OLED042_HEIGHT) return;

    int index = x + (y / 8) * OLED042_WIDTH;
    uint8_t mask = 1U << (y & 7);

    if (color) {
        oled->buffer[index] |= mask;
    } else {
        oled->buffer[index] &= (uint8_t)~mask;
    }
}

void oled_042_draw_hline(oled_042_t *oled, int x, int y, int w, bool color)
{
    for (int i = 0; i < w; i++) {
        oled_042_set_pixel(oled, x + i, y, color);
    }
}

void oled_042_draw_vline(oled_042_t *oled, int x, int y, int h, bool color)
{
    for (int i = 0; i < h; i++) {
        oled_042_set_pixel(oled, x, y + i, color);
    }
}

void oled_042_draw_rect(oled_042_t *oled, int x, int y, int w, int h, bool color)
{
    oled_042_draw_hline(oled, x, y, w, color);
    oled_042_draw_hline(oled, x, y + h - 1, w, color);
    oled_042_draw_vline(oled, x, y, h, color);
    oled_042_draw_vline(oled, x + w - 1, y, h, color);
}

void oled_042_fill_rect(oled_042_t *oled, int x, int y, int w, int h, bool color)
{
    for (int yy = 0; yy < h; yy++) {
        oled_042_draw_hline(oled, x, y + yy, w, color);
    }
}

void oled_042_draw_char(oled_042_t *oled, int x, int y, char c, bool color)
{
    if (!oled) return;

    const uint8_t *glyph = oled_042_get_glyph(c);

    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++) {
            if ((bits >> row) & 0x01) {
                oled_042_set_pixel(oled, x + col, y + row, color);
            }
        }
    }
}

void oled_042_draw_text(oled_042_t *oled, int x, int y, const char *text, bool color)
{
    if (!oled || !text) return;

    int cx = x;
    int cy = y;

    while (*text) {
        if (*text == '\n') {
            cy += 8;
            cx = x;
        } else {
            oled_042_draw_char(oled, cx, cy, *text, color);
            cx += 6;
            if (cx > OLED042_WIDTH - 6) {
                cy += 8;
                cx = x;
            }
        }
        text++;
    }
}

void oled_042_draw_char_2x(oled_042_t *oled, int x, int y, char c, bool color)
{
    if (!oled) return;

    const uint8_t *glyph = oled_042_get_glyph(c);

    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++) {
            if ((bits >> row) & 0x01) {
                int px = x + col * 2;
                int py = y + row * 2;

                oled_042_set_pixel(oled, px,     py,     color);
                oled_042_set_pixel(oled, px + 1, py,     color);
                oled_042_set_pixel(oled, px,     py + 1, color);
                oled_042_set_pixel(oled, px + 1, py + 1, color);
            }
        }
    }
}

void oled_042_draw_text_2x(oled_042_t *oled, int x, int y, const char *text, bool color)
{
    if (!oled || !text) return;

    int cx = x;
    while (*text) {
        oled_042_draw_char_2x(oled, cx, y, *text, color);
        cx += 12;
        text++;
    }
}
