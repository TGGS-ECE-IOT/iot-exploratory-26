#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

#define TAG "SSD1315"

#define OLED_I2C_PORT           I2C_NUM_0
#define OLED_I2C_SDA_GPIO       21
#define OLED_I2C_SCL_GPIO       22
#define OLED_I2C_FREQ_HZ        400000
#define OLED_I2C_ADDR           0x3C

#define OLED_WIDTH              128
#define OLED_HEIGHT             64
#define OLED_PAGE_COUNT         (OLED_HEIGHT / 8)
#define OLED_BUFFER_SIZE        (OLED_WIDTH * OLED_PAGE_COUNT)

#define OLED_CMD_MODE           0x00
#define OLED_DATA_MODE          0x40

static uint8_t oled_buffer[OLED_BUFFER_SIZE];

// 5x7 font for characters 32..127, 5 bytes per character
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E}, {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63}, {0x03,0x04,0x78,0x04,0x03}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78}, {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x08,0x14,0x54,0x54,0x3C},
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x44,0x3D,0x00}, {0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78}, {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7C}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C}, {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44}, {0x0C,0x50,0x50,0x50,0x3C}, {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7F,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x08,0x08,0x2A,0x1C,0x08}, {0x08,0x1C,0x2A,0x08,0x08}
};

static esp_err_t oled_write_command(uint8_t cmd)
{
    uint8_t payload[2] = { OLED_CMD_MODE, cmd };
    return i2c_master_write_to_device(OLED_I2C_PORT, OLED_I2C_ADDR, payload, sizeof(payload), pdMS_TO_TICKS(100));
}

static esp_err_t oled_write_data(const uint8_t *data, size_t len)
{
    uint8_t temp[17];
    temp[0] = OLED_DATA_MODE;

    while (len > 0) {
        size_t chunk = (len > 16) ? 16 : len;
        memcpy(&temp[1], data, chunk);
        esp_err_t ret = i2c_master_write_to_device(OLED_I2C_PORT, OLED_I2C_ADDR, temp, chunk + 1, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            return ret;
        }
        data += chunk;
        len -= chunk;
    }
    return ESP_OK;
}

static esp_err_t oled_set_cursor(uint8_t page, uint8_t column)
{
    esp_err_t ret;
    ret = oled_write_command(0xB0 | page);
    if (ret != ESP_OK) return ret;
    ret = oled_write_command(0x00 | (column & 0x0F));
    if (ret != ESP_OK) return ret;
    ret = oled_write_command(0x10 | ((column >> 4) & 0x0F));
    return ret;
}

static esp_err_t oled_update(void)
{
    for (uint8_t page = 0; page < OLED_PAGE_COUNT; page++) {
        ESP_ERROR_CHECK(oled_set_cursor(page, 0));
        ESP_ERROR_CHECK(oled_write_data(&oled_buffer[page * OLED_WIDTH], OLED_WIDTH));
    }
    return ESP_OK;
}

static void oled_clear_buffer(void)
{
    memset(oled_buffer, 0x00, sizeof(oled_buffer));
}

static void oled_fill_buffer(void)
{
    memset(oled_buffer, 0xFF, sizeof(oled_buffer));
}

static void oled_draw_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return;
    }

    uint16_t index = x + (y / 8) * OLED_WIDTH;
    uint8_t mask = 1U << (y % 8);

    if (on) {
        oled_buffer[index] |= mask;
    } else {
        oled_buffer[index] &= (uint8_t)~mask;
    }
}

static void oled_draw_hline(int x, int y, int w, bool on)
{
    for (int i = 0; i < w; i++) {
        oled_draw_pixel(x + i, y, on);
    }
}

static void oled_draw_vline(int x, int y, int h, bool on)
{
    for (int i = 0; i < h; i++) {
        oled_draw_pixel(x, y + i, on);
    }
}

static void oled_draw_rect(int x, int y, int w, int h, bool on)
{
    if (w <= 0 || h <= 0) return;
    oled_draw_hline(x, y, w, on);
    oled_draw_hline(x, y + h - 1, w, on);
    oled_draw_vline(x, y, h, on);
    oled_draw_vline(x + w - 1, y, h, on);
}

static void oled_draw_line(int x0, int y0, int x1, int y1, bool on)
{
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        oled_draw_pixel(x0, y0, on);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void oled_draw_char(int x, int y, char c, bool on)
{
    if (c < 32 || c > 127) {
        c = '?';
    }

    const uint8_t *glyph = font5x7[c - 32];

    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (bits & (1U << row)) {
                oled_draw_pixel(x + col, y + row, on);
            }
        }
    }
}

static void oled_draw_string(int x, int y, const char *text, bool on)
{
    while (*text) {
        oled_draw_char(x, y, *text, on);
        x += 6;
        text++;
        if (x > OLED_WIDTH - 6) {
            break;
        }
    }
}

static esp_err_t oled_init_i2c(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = OLED_I2C_SDA_GPIO,
        .scl_io_num = OLED_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = OLED_I2C_FREQ_HZ,
        .clk_flags = 0,
    };

    ESP_ERROR_CHECK(i2c_param_config(OLED_I2C_PORT, &conf));
    return i2c_driver_install(OLED_I2C_PORT, conf.mode, 0, 0, 0);
}

static esp_err_t oled_probe(void)
{
    uint8_t dummy = 0x00;
    return i2c_master_write_to_device(OLED_I2C_PORT, OLED_I2C_ADDR, &dummy, 1, pdMS_TO_TICKS(100));
}

static esp_err_t oled_init_display(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_ERROR_CHECK(oled_write_command(0xAE)); // display off
    ESP_ERROR_CHECK(oled_write_command(0xD5)); // set display clock divide
    ESP_ERROR_CHECK(oled_write_command(0x80));
    ESP_ERROR_CHECK(oled_write_command(0xA8)); // multiplex ratio
    ESP_ERROR_CHECK(oled_write_command(0x3F));
    ESP_ERROR_CHECK(oled_write_command(0xD3)); // display offset
    ESP_ERROR_CHECK(oled_write_command(0x00));
    ESP_ERROR_CHECK(oled_write_command(0x40)); // start line = 0
    ESP_ERROR_CHECK(oled_write_command(0x8D)); // charge pump
    ESP_ERROR_CHECK(oled_write_command(0x14));
    ESP_ERROR_CHECK(oled_write_command(0x20)); // memory mode
    ESP_ERROR_CHECK(oled_write_command(0x00)); // horizontal addressing
    ESP_ERROR_CHECK(oled_write_command(0xA1)); // segment remap
    ESP_ERROR_CHECK(oled_write_command(0xC8)); // COM scan dec
    ESP_ERROR_CHECK(oled_write_command(0xDA)); // COM pins
    ESP_ERROR_CHECK(oled_write_command(0x12));
    ESP_ERROR_CHECK(oled_write_command(0x81)); // contrast
    ESP_ERROR_CHECK(oled_write_command(0x7F));
    ESP_ERROR_CHECK(oled_write_command(0xD9)); // pre-charge
    ESP_ERROR_CHECK(oled_write_command(0xF1));
    ESP_ERROR_CHECK(oled_write_command(0xDB)); // VCOM detect
    ESP_ERROR_CHECK(oled_write_command(0x40));
    ESP_ERROR_CHECK(oled_write_command(0xA4)); // display follows RAM
    ESP_ERROR_CHECK(oled_write_command(0xA6)); // normal display
    ESP_ERROR_CHECK(oled_write_command(0x2E)); // deactivate scroll
    ESP_ERROR_CHECK(oled_write_command(0xAF)); // display on

    oled_clear_buffer();
    return oled_update();
}

static void screen_text_demo(void)
{
    oled_clear_buffer();
    oled_draw_rect(0, 0, 128, 64, true);
    oled_draw_string(8, 8, "GROVE SSD1315", true);
    oled_draw_string(14, 24, "Hello OLED", true);
    oled_draw_string(8, 40, "SDA21 SCL22", true);
    ESP_ERROR_CHECK(oled_update());
}

static void screen_shapes_demo(void)
{
    oled_clear_buffer();
    oled_draw_rect(0, 0, 128, 64, true);
    oled_draw_line(0, 0, 127, 63, true);
    oled_draw_line(127, 0, 0, 63, true);
    oled_draw_rect(34, 16, 60, 32, true);
    ESP_ERROR_CHECK(oled_update());
}

static void screen_counter_demo(int counter)
{
    char line[32];
    oled_clear_buffer();
    oled_draw_string(22, 8, "COUNTER PAGE", true);
    oled_draw_rect(10, 20, 108, 28, true);
    snprintf(line, sizeof(line), "COUNT: %d", counter);
    oled_draw_string(24, 30, line, true);
    ESP_ERROR_CHECK(oled_update());
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing I2C...");
    ESP_ERROR_CHECK(oled_init_i2c());

    ESP_LOGI(TAG, "Probing OLED at 0x%02X...", OLED_I2C_ADDR);
    esp_err_t probe_ret = oled_probe();
    if (probe_ret != ESP_OK) {
        ESP_LOGE(TAG, "OLED probe failed. Check wiring and I2C address. err=0x%x", probe_ret);
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ESP_LOGI(TAG, "Initializing display...");
    ESP_ERROR_CHECK(oled_init_display());

    int counter = 0;
    while (1) {
        screen_text_demo();
        vTaskDelay(pdMS_TO_TICKS(2000));

        screen_shapes_demo();
        vTaskDelay(pdMS_TO_TICKS(2000));

        for (int i = 0; i < 5; i++) {
            screen_counter_demo(counter++);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
