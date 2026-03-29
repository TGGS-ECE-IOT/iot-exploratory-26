# 0.91" OLED (SSD1306 128x32) Tutorial (ESP32 + PlatformIO + ESP-IDF)

## 1) Overview

The **0.91-inch OLED module** is a small monochrome display based on the **SSD1306 controller**.

Common characteristics:

- Resolution: **128 × 32 pixels**
- Interface: **I2C (SCL/SDA)**
- Low power consumption
- High contrast (self-emitting pixels, no backlight)

This module is ideal for:

- Sensor data display
- Debug output
- UI indicators (status, icons)
- IoT device dashboards

---

## 2) Pinout

Typical pins:

| Pin | Function |
|---|---|
| GND | Ground |
| VCC | Power (3.3V–5V depending on module) |
| SCL | I2C clock |
| SDA | I2C data |

---

## 3) Wiring with ESP32

| OLED | ESP32 |
|---|---|
| GND | GND |
| VCC | 3.3V |
| SCL | GPIO22 |
| SDA | GPIO21 |

> ESP32 default I2C pins: SDA = GPIO21, SCL = GPIO22

---

## 4) I2C Address

Typical SSD1306 I2C addresses:

- `0x3C` (most common)
- `0x3D` (less common)

---

## 5) PlatformIO Setup

### 5.1 Folder structure

```text
oled_esp32/
├── include/
├── lib/
├── src/
│   └── main.c
└── platformio.ini
```

### 5.2 `platformio.ini`

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
monitor_speed = 115200
```

---

## 6) Required Components

We will use:

- ESP-IDF I2C driver
- A minimal SSD1306 driver (custom)

---

## 7) I2C Initialization

### `i2c_init()`

```c
#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

void i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}
```

---

## 8) SSD1306 Basic Driver

### 8.1 Constants

```c
#define OLED_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
```

---

### 8.2 Send command/data

```c
esp_err_t oled_cmd(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};
    return i2c_master_write_to_device(I2C_MASTER_NUM, OLED_ADDR, data, 2, 1000 / portTICK_PERIOD_MS);
}

esp_err_t oled_data(uint8_t *data, size_t len) {
    uint8_t buffer[len + 1];
    buffer[0] = 0x40;
    memcpy(&buffer[1], data, len);
    return i2c_master_write_to_device(I2C_MASTER_NUM, OLED_ADDR, buffer, len + 1, 1000 / portTICK_PERIOD_MS);
}
```

---

## 9) OLED Initialization

```c
void oled_init(void) {
    oled_cmd(0xAE); // display off
    oled_cmd(0x20); // memory mode
    oled_cmd(0x00); // horizontal addressing
    oled_cmd(0xB0);
    oled_cmd(0xC8);
    oled_cmd(0x00);
    oled_cmd(0x10);
    oled_cmd(0x40);
    oled_cmd(0x81);
    oled_cmd(0xFF);
    oled_cmd(0xA1);
    oled_cmd(0xA6);
    oled_cmd(0xA8);
    oled_cmd(0x1F);
    oled_cmd(0xA4);
    oled_cmd(0xD3);
    oled_cmd(0x00);
    oled_cmd(0xD5);
    oled_cmd(0xF0);
    oled_cmd(0xD9);
    oled_cmd(0x22);
    oled_cmd(0xDA);
    oled_cmd(0x02);
    oled_cmd(0xDB);
    oled_cmd(0x20);
    oled_cmd(0x8D);
    oled_cmd(0x14);
    oled_cmd(0xAF); // display on
}
```

---

## 10) Clear Display

```c
void oled_clear(void) {
    uint8_t zero[128] = {0};
    for (int page = 0; page < 4; page++) {
        oled_cmd(0xB0 + page);
        oled_cmd(0x00);
        oled_cmd(0x10);
        oled_data(zero, 128);
    }
}
```

---

## 11) Simple Text Display (Demo Pattern)

For simplicity, we will display a pattern instead of full font rendering.

```c
void oled_test_pattern(void) {
    uint8_t buffer[128];

    for (int i = 0; i < 128; i++) {
        buffer[i] = 0xAA; // pattern
    }

    for (int page = 0; page < 4; page++) {
        oled_cmd(0xB0 + page);
        oled_cmd(0x00);
        oled_cmd(0x10);
        oled_data(buffer, 128);
    }
}
```

---

## 12) Main Application

### `src/main.c`

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include <string.h>

// include previous functions here

void app_main(void) {
    i2c_master_init();
    oled_init();
    oled_clear();

    while (1) {
        oled_test_pattern();
        vTaskDelay(pdMS_TO_TICKS(1000));
        oled_clear();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## 13) Extending to Text Rendering

To display real text, you need:

- Font table (e.g., 5x7 ASCII font)
- Character rendering function
- Cursor control

Typical approach:

1. Store font in array
2. Convert each character into pixel columns
3. Write to display buffer

---

## 14) Common Issues

### Nothing displayed

- Wrong I2C address
- Incorrect wiring
- Missing pull-up resistors

### Garbled output

- Wrong initialization sequence
- Incorrect page addressing

### Display too dim

- Adjust contrast (0x81 command)

---

## 15) Practical Usage

This OLED is ideal for your sensor node:

Display:

- Distance (HC-SR04)
- Temperature / humidity (DHT22)
- Air quality status (MQ-135)
- WiFi / MQTT status

Example UI layout:

```text
Temp: 28.3C
Hum : 65%
Air : Good
Dist: 120 cm
```

---

## 16) Summary

The 0.91" SSD1306 OLED is a compact and powerful display for ESP32 projects.

Key points:

- Uses I2C communication
- Requires proper initialization sequence
- Easy to integrate with sensor dashboards
- Works well with ESP-IDF using I2C driver

It is highly recommended for your IoT workshop as a **local visualization interface**.

