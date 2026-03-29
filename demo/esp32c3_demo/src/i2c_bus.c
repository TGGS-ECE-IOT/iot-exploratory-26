#include "i2c_bus.h"

static bool s_i2c_inited = false;

esp_err_t app_i2c_init(void)
{
    if (s_i2c_inited) {
        return ESP_OK;
    }

    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BOARD_I2C_SDA,
        .scl_io_num = BOARD_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BOARD_I2C_FREQ_HZ,
        .clk_flags = 0
    };

    esp_err_t err = i2c_param_config(BOARD_I2C_PORT, &cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = i2c_driver_install(BOARD_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        return err;
    }

    s_i2c_inited = true;
    return ESP_OK;
}

i2c_port_t app_i2c_port(void)
{
    return BOARD_I2C_PORT;
}
