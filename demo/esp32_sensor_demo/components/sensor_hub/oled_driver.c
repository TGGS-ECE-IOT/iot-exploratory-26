#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/i2c.h"

#include "esp_err.h"
#include "rom/ets_sys.h"

#if defined(__has_include)
#if __has_include("u8g2.h")
#include "u8g2.h"
#elif __has_include("clib/u8g2.h")
#include "clib/u8g2.h"
#else
#error "U8g2 header not found. Install U8g2 or add include path."
#endif
#else
#include "u8g2.h"
#endif

#include "oled_driver.h"
#include "pin_config.h"

#define I2C_PORT     I2C_NUM_0
#define I2C_FREQ_HZ  400000
#define OLED_ADDR    0x3C
#define OLED_PAGE_H  8
#define OLED_WIDTH   128

static u8g2_t g_u8g2;
static uint8_t g_i2c_buf[32];
static uint8_t g_i2c_buf_len;

static uint8_t flush_i2c_buf(void) {
    if (g_i2c_buf_len == 0) {
        return 1;
    }
    if (i2c_master_write_to_device(I2C_PORT, OLED_ADDR, g_i2c_buf, g_i2c_buf_len, pdMS_TO_TICKS(100)) != ESP_OK) {
        g_i2c_buf_len = 0;
        return 0;
    }
    g_i2c_buf_len = 0;
    return 1;
}

static uint8_t u8x8_byte_esp32_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    (void)u8x8;

    switch (msg) {
        case U8X8_MSG_BYTE_INIT:
            return 1;
        case U8X8_MSG_BYTE_START_TRANSFER:
            g_i2c_buf_len = 0;
            return 1;
        case U8X8_MSG_BYTE_SEND: {
            uint8_t *data = (uint8_t *)arg_ptr;
            while (arg_int > 0) {
                if (g_i2c_buf_len >= sizeof(g_i2c_buf) && !flush_i2c_buf()) {
                    return 0;
                }
                g_i2c_buf[g_i2c_buf_len++] = *data++;
                arg_int--;
            }
            return 1;
        }
        case U8X8_MSG_BYTE_END_TRANSFER:
            return flush_i2c_buf();
        default:
            return 0;
    }
}

static uint8_t u8x8_gpio_and_delay_esp32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    (void)u8x8;
    (void)arg_ptr;

    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            return 1;
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            return 1;
        case U8X8_MSG_DELAY_10MICRO:
            ets_delay_us(10);
            return 1;
        case U8X8_MSG_DELAY_100NANO:
            return 1;
        default:
            return 1;
    }
}

void oled_clear_buffer(void) {
    u8g2_ClearBuffer(&g_u8g2);
}

void oled_draw_text(int x, int page, const char *text) {
    if (text == NULL) {
        return;
    }
    u8g2_DrawStr(&g_u8g2, x, (page * OLED_PAGE_H) + 7, text);
}

void oled_draw_text_centered(int y, const char *text) {
    if (text == NULL) {
        return;
    }
    int w = u8g2_GetStrWidth(&g_u8g2, text);
    int x = (OLED_WIDTH - w) / 2;
    if (x < 0) {
        x = 0;
    }
    u8g2_DrawStr(&g_u8g2, x, y, text);
}

void oled_draw_text_right(int y, const char *text) {
    if (text == NULL) {
        return;
    }
    int w = u8g2_GetStrWidth(&g_u8g2, text);
    int x = OLED_WIDTH - w;
    if (x < 0) {
        x = 0;
    }
    u8g2_DrawStr(&g_u8g2, x, y, text);
}

void oled_draw_xbm(int x, int y, int width, int height, const uint8_t *bitmap) {
    if (bitmap == NULL) {
        return;
    }
    u8g2_DrawXBM(&g_u8g2, x, y, width, height, bitmap);
}

void oled_draw_line(int x0, int y0, int x1, int y1) {
    u8g2_DrawLine(&g_u8g2, x0, y0, x1, y1);
}

void oled_draw_circle(int x, int y, int radius) {
    u8g2_DrawCircle(&g_u8g2, x, y, radius, U8G2_DRAW_ALL);
}

void oled_set_font_small(void) {
    u8g2_SetFont(&g_u8g2, u8g2_font_5x8_tr);
}

void oled_set_font_large(void) {
    u8g2_SetFont(&g_u8g2, u8g2_font_10x20_tf);
}

void oled_refresh(void) {
    u8g2_SendBuffer(&g_u8g2);
}

void oled_driver_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_OLED_SDA,
        .scl_io_num = PIN_OLED_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));

    esp_err_t err = i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&g_u8g2, U8G2_R0, u8x8_byte_esp32_i2c, u8x8_gpio_and_delay_esp32);
    u8x8_SetI2CAddress(&g_u8g2.u8x8, OLED_ADDR << 1);
    u8g2_InitDisplay(&g_u8g2);
    u8g2_SetPowerSave(&g_u8g2, 0);
    oled_set_font_small();

    oled_clear_buffer();
    oled_refresh();
}
