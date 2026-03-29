#include "oled.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "OLED";

#define OLED_CMD_MODE    0x00
#define OLED_DATA_MODE   0x40

static const uint8_t glyph_space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t glyph_dot[5] = {0x00, 0x40, 0x60, 0x00, 0x00};
static const uint8_t glyph_colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
static const uint8_t glyph_excl[5] = {0x00, 0x00, 0x5F, 0x00, 0x00};
static const uint8_t glyph_dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
static const uint8_t glyph_underscore[5] = {0x40, 0x40, 0x40, 0x40, 0x40};
static const uint8_t glyph_qmark[5] = {0x02, 0x01, 0x51, 0x09, 0x06};

static const uint8_t glyph_digits[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
};

static const uint8_t glyph_upper[26][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43},
};

static const uint8_t *oled_get_glyph(char c)
{
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }

    if (c >= 'A' && c <= 'Z') {
        return glyph_upper[c - 'A'];
    }
    if (c >= '0' && c <= '9') {
        return glyph_digits[c - '0'];
    }

    switch (c) {
        case ' ':
            return glyph_space;
        case '.':
            return glyph_dot;
        case ':':
            return glyph_colon;
        case '!':
            return glyph_excl;
        case '-':
            return glyph_dash;
        case '_':
            return glyph_underscore;
        default:
            return glyph_qmark;
    }
}

static void oled_send_cmd(oled_config_t *config, uint8_t cmd)
{
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (config->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, OLED_CMD_MODE, true);
    i2c_master_write_byte(handle, cmd, true);
    i2c_master_stop(handle);
    i2c_master_cmd_begin(config->i2c_port, handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(handle);
}

static void oled_send_data(oled_config_t *config, uint8_t data)
{
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (config->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, OLED_DATA_MODE, true);
    i2c_master_write_byte(handle, data, true);
    i2c_master_stop(handle);
    i2c_master_cmd_begin(config->i2c_port, handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(handle);
}

void oled_init(oled_config_t *config)
{
    config->sda_gpio = OLED_SDA_GPIO;
    config->scl_gpio = OLED_SCL_GPIO;
    config->i2c_port = OLED_I2C_PORT;
    config->address = OLED_ADDR;
    memset(config->buffer, 0, sizeof(config->buffer));

    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sda_gpio,
        .scl_io_num = config->scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000
    };
    i2c_param_config(config->i2c_port, &i2c_cfg);
    i2c_driver_install(config->i2c_port, I2C_MODE_MASTER, 0, 0, 0);

    vTaskDelay(pdMS_TO_TICKS(100));

    oled_send_cmd(config, 0xAE);
    oled_send_cmd(config, 0xD5);
    oled_send_cmd(config, 0x80);
    oled_send_cmd(config, 0xA8);
    oled_send_cmd(config, 0x1F);
    oled_send_cmd(config, 0xD3);
    oled_send_cmd(config, 0x00);
    oled_send_cmd(config, 0x40);
    oled_send_cmd(config, 0x81);
    oled_send_cmd(config, 0x8F);
    oled_send_cmd(config, 0xA4);
    oled_send_cmd(config, 0xA6);
    oled_send_cmd(config, 0x20);
    oled_send_cmd(config, 0x00);
    oled_send_cmd(config, 0xA1);
    oled_send_cmd(config, 0xC8);
    oled_send_cmd(config, 0xDA);
    oled_send_cmd(config, 0x02);
    oled_send_cmd(config, 0xD9);
    oled_send_cmd(config, 0xF1);
    oled_send_cmd(config, 0xDB);
    oled_send_cmd(config, 0x40);
    oled_send_cmd(config, 0x8D);
    oled_send_cmd(config, 0x14);
    oled_send_cmd(config, 0x2E);
    oled_send_cmd(config, 0xAF);

    ESP_LOGI(TAG, "OLED initialized on I2C%d at addr 0x%02X", config->i2c_port, config->address);
}

void oled_clear(oled_config_t *config)
{
    memset(config->buffer, 0, sizeof(config->buffer));
    for (uint8_t page = 0; page < 4; page++) {
        oled_send_cmd(config, 0xB0 + page);
        oled_send_cmd(config, 0x00);
        oled_send_cmd(config, 0x10);
        for (uint8_t col = 0; col < 128; col++) {
            oled_send_data(config, 0x00);
        }
    }
}

static void oled_draw_char(oled_config_t *config, uint8_t x, uint8_t page, char c)
{
    if (page >= 4) {
        return;
    }
    const uint8_t *glyph = oled_get_glyph(c);
    
    for (uint8_t i = 0; i < 5; i++) {
        if (x + i < 128) {
            config->buffer[page * 128 + x + i] = glyph[i];
        }
    }
    if (x + 5 < 128) {
        config->buffer[page * 128 + x + 5] = 0;
    }
}

static void oled_set_pixel(oled_config_t *config, uint8_t x, uint8_t y, uint8_t on)
{
    uint16_t idx;
    uint8_t mask;

    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }

    idx = (uint16_t)(y / 8) * OLED_WIDTH + x;
    mask = (uint8_t)(1U << (y % 8));

    if (on) {
        config->buffer[idx] |= mask;
    } else {
        config->buffer[idx] &= (uint8_t)~mask;
    }
}

void oled_draw_str(oled_config_t *config, uint8_t x, uint8_t y, const char *str)
{
    uint8_t page = y / 8;
    if (page >= 4) {
        return;
    }
    uint8_t col = x;
    
    while (*str && col < 128) {
        oled_draw_char(config, col, page, *str);
        col += 6;
        str++;
    }
    
    for (uint8_t p = 0; p < 4; p++) {
        oled_send_cmd(config, 0xB0 + p);
        oled_send_cmd(config, 0x00);
        oled_send_cmd(config, 0x10);
        for (uint8_t c = 0; c < 128; c++) {
            oled_send_data(config, config->buffer[p * 128 + c]);
        }
    }
}

void oled_draw_str_big(oled_config_t *config, uint8_t x, uint8_t y, const char *str)
{
    const uint8_t scale = 2;
    uint8_t col = x;

    while (*str && col < OLED_WIDTH) {
        const uint8_t *glyph = oled_get_glyph(*str);

        for (uint8_t gx = 0; gx < 5; gx++) {
            for (uint8_t gy = 0; gy < 8; gy++) {
                if (glyph[gx] & (1U << gy)) {
                    for (uint8_t sx = 0; sx < scale; sx++) {
                        for (uint8_t sy = 0; sy < scale; sy++) {
                            oled_set_pixel(config,
                                           (uint8_t)(col + gx * scale + sx),
                                           (uint8_t)(y + gy * scale + sy),
                                           1);
                        }
                    }
                }
            }
        }

        col = (uint8_t)(col + (5 * scale) + 1);
        str++;
    }

    oled_update_display(config);
}

void oled_draw_int(oled_config_t *config, uint8_t x, uint8_t y, int value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    oled_draw_str(config, x, y, buf);
}

void oled_update_display(oled_config_t *config)
{
    for (uint8_t page = 0; page < 4; page++) {
        oled_send_cmd(config, 0xB0 + page);
        oled_send_cmd(config, 0x00);
        oled_send_cmd(config, 0x10);
        for (uint8_t col = 0; col < 128; col++) {
            oled_send_data(config, config->buffer[page * 128 + col]);
        }
    }
}

void oled_draw_bitmap_full(oled_config_t *config, const uint8_t *bitmap)
{
    memcpy(config->buffer, bitmap, sizeof(config->buffer));
    oled_update_display(config);
}
