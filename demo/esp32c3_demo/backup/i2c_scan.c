#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "driver/gpio.h"

#include "esp_err.h"
#include "esp_system.h"

#define I2C_PORT        I2C_NUM_0
#define I2C_FREQ_HZ     100000

static esp_err_t i2c_init_on_pins(gpio_num_t sda, gpio_num_t scl)
{
    i2c_driver_delete(I2C_PORT);

    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
        .clk_flags = 0
    };

    esp_err_t err = i2c_param_config(I2C_PORT, &cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    return ESP_OK;
}

static void i2c_scan_once(const char *label, gpio_num_t sda, gpio_num_t scl)
{
    printf("\n");
    printf("====================================\n");
    printf("Scanning %s\n", label);
    printf("SDA = %d, SCL = %d\n", (int)sda, (int)scl);
    fflush(stdout);

    esp_err_t err = i2c_init_on_pins(sda, scl);
    if (err != ESP_OK) {
        printf("I2C init failed: %s\n", esp_err_to_name(err));
        fflush(stdout);
        return;
    }

    int found = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        if (cmd == NULL) {
            printf("Failed to create I2C command link\n");
            fflush(stdout);
            return;
        }

        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(30));
        i2c_cmd_link_delete(cmd);

        if (err == ESP_OK) {
            printf("Found device at 0x%02X\n", addr);
            fflush(stdout);
            found++;
        }
    }

    if (found == 0) {
        printf("No I2C devices found on %s\n", label);
    } else {
        printf("Total devices found on %s: %d\n", label, found);
    }
    fflush(stdout);
}

void app_main(void)
{
    printf("\n");
    printf("BOOT OK - app_main started\n");
    fflush(stdout);

    while (1) {
        i2c_scan_once("GPIO5/GPIO6", GPIO_NUM_5, GPIO_NUM_6);
        vTaskDelay(pdMS_TO_TICKS(1000));

        i2c_scan_once("GPIO8/GPIO9", GPIO_NUM_8, GPIO_NUM_9);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
