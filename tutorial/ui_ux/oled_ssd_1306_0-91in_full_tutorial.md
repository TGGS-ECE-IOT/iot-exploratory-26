# 0.91" I2C OLED (SSD1306 128x32) Tutorial
## ESP32 + PlatformIO + ESP-IDF

---

## 1) Overview

The **0.91" OLED module** uses the **SSD1306 controller** and communicates via **I2C**.

Specifications:

- Resolution: **128 × 32 pixels**
- Interface: **I2C**
- Display type: **Monochrome (1-bit)**
- Controller: **SSD1306**

Applications:

- Sensor dashboard
- Debug display
- IoT device UI

---

## 2) Basic of I2C Communication

I2C (Inter-Integrated Circuit) is a **2-wire serial protocol**.

### Signals

| Signal | Description |
|------|------------|
| SDA | Data line |
| SCL | Clock line |

### Key concepts

- Master (ESP32) controls communication
- Slave (OLED) has an address
- Data sent in bytes

### Typical OLED I2C Address

- `0x3C` (most common)
- `0x3D` (alternate)

---

## 3) Pinout and Wiring

| OLED | ESP32 |
|------|------|
| GND | GND |
| VCC | 3.3V |
| SCL | GPIO22 |
| SDA | GPIO21 |

---

## 4) PlatformIO Setup

### platformio.ini

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
monitor_speed = 115200
```

---

## 5) I2C Initialization

```c
#include "driver/i2c.h"

#define I2C_SCL 22
#define I2C_SDA 21
#define I2C_PORT I2C_NUM_0

void i2c_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };

    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}
```

---

## 6) SSD1306 Command Basics

The OLED uses two types of data:

| Type | Prefix |
|------|-------|
| Command | 0x00 |
| Data | 0x40 |

### Send command

```c
void oled_cmd(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};
    i2c_master_write_to_device(I2C_PORT, 0x3C, data, 2, 1000 / portTICK_PERIOD_MS);
}
```

### Send data

```c
void oled_data(uint8_t *data, size_t len) {
    uint8_t buffer[len + 1];
    buffer[0] = 0x40;
    memcpy(&buffer[1], data, len);
    i2c_master_write_to_device(I2C_PORT, 0x3C, buffer, len + 1, 1000 / portTICK_PERIOD_MS);
}
```

---

## 7) OLED Initialization Sequence

```c
void oled_init() {
    oled_cmd(0xAE);
    oled_cmd(0x20); oled_cmd(0x00);
    oled_cmd(0xB0);
    oled_cmd(0xC8);
    oled_cmd(0x00);
    oled_cmd(0x10);
    oled_cmd(0x40);
    oled_cmd(0x81); oled_cmd(0xFF);
    oled_cmd(0xA1);
    oled_cmd(0xA6);
    oled_cmd(0xA8); oled_cmd(0x1F);
    oled_cmd(0xD3); oled_cmd(0x00);
    oled_cmd(0xD5); oled_cmd(0xF0);
    oled_cmd(0xD9); oled_cmd(0x22);
    oled_cmd(0xDA); oled_cmd(0x02);
    oled_cmd(0xDB); oled_cmd(0x20);
    oled_cmd(0x8D); oled_cmd(0x14);
    oled_cmd(0xAF);
}
```

---

## 8) Framebuffer

```c
static uint8_t buffer[512];
```

### Clear

```c
void clear_buffer() {
    memset(buffer, 0, sizeof(buffer));
}
```

### Update display

```c
void oled_update() {
    for (int page = 0; page < 4; page++) {
        oled_cmd(0xB0 + page);
        oled_cmd(0x00);
        oled_cmd(0x10);
        oled_data(&buffer[page * 128], 128);
    }
}
```

---

## 9) Draw Pixel

```c
void draw_pixel(int x, int y) {
    int index = x + (y / 8) * 128;
    buffer[index] |= (1 << (y % 8));
}
```

---

## 10) Text Rendering (TIS-620 Monospace)

### Font structure

```c
extern const uint8_t font8x8[256][8];
```

### Draw character

```c
void draw_char(int x, int y, uint8_t ch) {
    for (int r = 0; r < 8; r++) {
        uint8_t row = font8x8[ch][r];
        for (int c = 0; c < 8; c++) {
            if (row & (1 << (7 - c)))
                draw_pixel(x + c, y + r);
        }
    }
}
```

### Draw string

```c
void draw_text(int x, int y, const uint8_t *str) {
    while (*str) {
        draw_char(x, y, *str++);
        x += 8;
    }
}
```

---

## 11) 2-Line Display

```c
void show_2lines() {
    clear_buffer();
    draw_text(0, 0, (uint8_t*)"TEMP 28.5C");
    draw_text(0, 16, (uint8_t*)"HUM  65%");
    oled_update();
}
```

---

## 12) 3-Line Display

```c
void show_3lines() {
    clear_buffer();
    draw_text(0, 0, (uint8_t*)"Temp: 28C");
    draw_text(0, 8, (uint8_t*)"Hum : 65%");
    draw_text(0, 16, (uint8_t*)"Air : Good");
    oled_update();
}
```

---

## 13) Thai Text (TIS-620)

Example byte string:

```c
uint8_t thai[] = {0xCA,0xC7,0xD1,0xCA,0xB4,0xD5,0x00};
```

Then:

```c
draw_text(0,0, thai);
```

> Requires prepared Thai bitmap font

---

## 14) Image Display (128x32)

### Convert grayscale image

Python script:

```python
from PIL import Image

img = Image.open("input.png").convert("L")
img = img.resize((128,32))
img = img.point(lambda p: 255 if p>128 else 0, mode='1')

pixels = img.load()
output=[]

for page in range(4):
    for x in range(128):
        byte=0
        for b in range(8):
            y=page*8+b
            if pixels[x,y]:
                byte |= (1<<b)
        output.append(byte)

print(output)
```

---

### Display image

```c
extern const uint8_t img[512];

void show_image() {
    memcpy(buffer, img, 512);
    oled_update();
}
```

---

## 15) Main Example

```c
void app_main() {
    i2c_init();
    oled_init();

    while(1) {
        show_2lines();
        vTaskDelay(pdMS_TO_TICKS(2000));

        show_3lines();
        vTaskDelay(pdMS_TO_TICKS(2000));

        show_image();
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
```

---

## 16) Summary

- SSD1306 uses I2C communication
- Requires framebuffer (512 bytes)
- Supports text via bitmap font
- Can display Thai (TIS-620) if font prepared
- Images must be converted to 1-bit format

This module is ideal for IoT dashboards and embedded UI.

