# Grove SSD1315 OLED 0.96" (Yellow & Blue) with ESP32 via I2C

This tutorial explains how to use the **Seeed Grove - SSD1315 OLED Display 0.96 inch (Yellow & Blue)** with an **ESP32** over **I2C** using:

- **SDA = GPIO21**
- **SCL = GPIO22**

The module is a **128 × 64** two-color OLED display based on the **SSD1315** controller. Seeed documents it as a blue/yellow bi-color display, supporting both **3.3 V and 5 V**, and both **I2C and SPI** interfaces. For this tutorial, we use **I2C only**.

---

## 1) What this module is

The Grove OLED Yellow & Blue display is a small monochrome graphical display where the top region appears yellow and the lower region appears blue because of the panel construction. Logically, it is still drawn as a normal **128 × 64 pixel framebuffer**.

Typical uses:

- sensor values
- Wi-Fi status
- menu screens
- simple icons and logos
- debugging output without a serial monitor

---

## 2) Important notes before wiring

### Resolution
Use **128 × 64** in software.

### Power level
The Grove module supports **3.3 V and 5 V** power. Since ESP32 is a 3.3 V device, powering the display from **3.3 V** is the safest choice.

### I2C address
Most examples for this class of display use **0x3C**. However, Seeed also mentions that the address is changeable, and there is community confusion between **0x3C** and values written in 8-bit form such as **0x78**.

For ESP32 code, use the **7-bit I2C address**, which is usually:

```c
0x3C
```

If the display does not respond, run an I2C scanner first.

---

## 3) ESP32 wiring

### Direct wiring

| Grove OLED pin | ESP32 pin |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

If you use a Grove-to-jumper cable, connect the Grove I2C lines to the same ESP32 pins.

---

## 4) Software approach used in this tutorial

This example is written in **C for ESP-IDF / PlatformIO (framework = espidf)**.

The code includes:

- ESP32 I2C master initialization
- SSD1315 command write
- framebuffer write
- clear screen
- draw pixel
- draw character (5x7 font)
- draw string
- draw line
- draw rectangle
- draw simple demo pages

This is a **bare-metal style driver example**. It does not depend on Arduino libraries such as U8g2.

---

## 5) Project structure

Suggested files:

```text
src/
  main.c
```

---

## 6) PlatformIO configuration example

Example `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
monitor_speed = 115200
```

---

## 7) How the display memory works

The OLED uses a page-oriented memory layout:

- width = 128 columns
- height = 64 rows
- pages = 8
- each page holds 8 vertical pixels per column

Framebuffer size:

```text
128 × 64 / 8 = 1024 bytes
```

So the display buffer in C is:

```c
uint8_t oled_buffer[1024];
```

---

## 8) Minimal initialization sequence

A typical initialization does the following:

- turn display off
- set addressing mode
- configure multiplex / COM pins
- configure contrast
- configure charge pump or power mode
- set segment remap and COM scan direction
- turn display on

SSD1315 is very similar in practice to SSD1306 for many 128 × 64 OLED modules, so a standard SSD1306-style init sequence usually works well for this panel.

---

## 9) Build and upload

1. Create a PlatformIO project for ESP32 using ESP-IDF.
2. Replace `src/main.c` with the example code.
3. Wire the display to ESP32:
   - SDA → GPIO21
   - SCL → GPIO22
4. Build and upload.
5. Open Serial Monitor at **115200 baud**.

---

## 10) What the example shows

The example cycles through several screens:

1. **Text page**
   - title
   - “Hello OLED”
   - I2C pin summary

2. **Shapes page**
   - frame border
   - diagonal lines
   - rectangle

3. **Counter page**
   - increasing counter value

---

## 11) Troubleshooting

### Nothing appears on screen
- Check **3V3** and **GND**.
- Confirm **SDA = GPIO21** and **SCL = GPIO22**.
- Confirm the I2C address is **0x3C**.
- Try an I2C scanner.
- Make sure the module is still configured for **I2C**, not SPI hardware mode.

### Garbled output
- Confirm software uses **128 × 64**.
- Confirm initialization is done before drawing.
- Clear buffer before drawing a new screen.

### Screen lights up but stays blank
- Verify `oled_update()` is called after drawing.
- Verify page and column addressing commands are correct.

---

## 12) Optional extension ideas

After the basic example works, you can extend it with:

- sensor dashboard page
- Wi-Fi connection status page
- menu system with push buttons
- scrolling text
- bitmap icons
- Thai font renderer
- animation frames

---

## 13) Example code

Use the accompanying `main.c` file.

Main features implemented:

- I2C driver for ESP32
- SSD1315 initialization
- 1024-byte framebuffer
- text drawing with built-in 5x7 font
- line and rectangle drawing
- multi-screen demo loop

---

## 14) I2C scanner snippet

If the display is not detected, temporarily test with this scanner logic pattern:

```c
for (uint8_t addr = 1; addr < 127; addr++) {
    // probe address and print matches
}
```

If your scanner finds `0x3C`, use that value in the display driver.

---

## 15) Summary

This OLED is a compact and useful display for ESP32 projects. With I2C on **GPIO21/GPIO22**, it is easy to add a local UI for status, sensor readings, and simple graphics.

For ESP32 projects, the most practical setup is:

- **VCC = 3.3V**
- **SDA = GPIO21**
- **SCL = GPIO22**
- **I2C address = 0x3C** in most cases
- **resolution = 128 × 64**

