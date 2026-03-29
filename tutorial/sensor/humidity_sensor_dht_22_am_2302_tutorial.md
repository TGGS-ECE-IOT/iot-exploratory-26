# AM2302 (DHT22) Temperature & Humidity Sensor Tutorial (ESP32 + PlatformIO + ESP-IDF)

## 1) What is AM2302 (DHT22)?

The **AM2302**, also known as **DHT22**, is a digital sensor used to measure:

- Temperature
- Relative humidity

It provides calibrated digital output, making it easy to interface with microcontrollers like the **ESP32**.

---

## 2) Key Features

Typical specifications:

- Operating voltage: **3.3 V – 6 V**
- Temperature range: **-40°C to 80°C**
- Temperature accuracy: **±0.5°C**
- Humidity range: **0–100% RH**
- Humidity accuracy: **±2–5% RH**
- Sampling rate: **~0.5 Hz (once every 2 seconds)**

> Important: Do not read the sensor faster than once every 2 seconds.

---

## 3) Pinout

AM2302 (DHT22) has 3 or 4 pins depending on module type:

| Pin | Function |
|---|---|
| VCC | Power (3.3V or 5V) |
| DATA | Digital data signal |
| GND | Ground |

### Pull-up resistor

The DATA line requires a **pull-up resistor (typically 4.7kΩ – 10kΩ)** to VCC.

Many modules already include this resistor.

---

## 4) Working Principle

The DHT22 uses a proprietary **single-wire protocol**.

Basic sequence:

1. MCU sends a **start signal** (pull DATA low for ≥1 ms)
2. Sensor responds with acknowledgment
3. Sensor sends **40 bits of data**:

```text
16 bits humidity + 16 bits temperature + 8 bits checksum
```

Each bit is encoded by pulse width timing.

---

## 5) Wiring with ESP32

| DHT22 | ESP32 |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| DATA | Any GPIO (e.g., GPIO 4) |

Example:

- DATA → GPIO4

---

## 6) PlatformIO Project Setup

### 6.1 Folder structure

```text
dht22_esp32/
├── include/
├── lib/
├── src/
│   └── main.c
└── platformio.ini
```

### 6.2 platformio.ini

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
monitor_speed = 115200
```

---

## 7) Implementation Approach

There are two ways to use DHT22 on ESP32:

### Option A (Recommended)
Use a library (stable and easier)

### Option B
Implement timing manually (educational but sensitive to timing errors)

This tutorial uses **Option A** with a lightweight driver.

---

## 8) Using DHT22 Library (ESP-IDF)

### 8.1 Add library

Create a folder:

```text
lib/dht/
```

Add a simple DHT driver (example minimal implementation).

---

### 8.2 `lib/dht/dht.h`

```c
#pragma once

#include "driver/gpio.h"

int dht_read(gpio_num_t pin, float *temperature, float *humidity);
```

---

### 8.3 `lib/dht/dht.c`

```c
#include "dht.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#define DHT_TIMEOUT 10000

static int wait_for_level(gpio_num_t pin, int level, int timeout_us) {
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(pin) != level) {
        if ((esp_timer_get_time() - start) > timeout_us) {
            return -1;
        }
    }
    return 0;
}

int dht_read(gpio_num_t pin, float *temperature, float *humidity) {
    uint8_t data[5] = {0};

    // Start signal
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    esp_rom_delay_us(1100);

    gpio_set_level(pin, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    // Sensor response
    if (wait_for_level(pin, 0, DHT_TIMEOUT) < 0) return -1;
    if (wait_for_level(pin, 1, DHT_TIMEOUT) < 0) return -1;
    if (wait_for_level(pin, 0, DHT_TIMEOUT) < 0) return -1;

    // Read 40 bits
    for (int i = 0; i < 40; i++) {
        if (wait_for_level(pin, 1, DHT_TIMEOUT) < 0) return -1;

        int64_t start = esp_timer_get_time();
        if (wait_for_level(pin, 0, DHT_TIMEOUT) < 0) return -1;
        int64_t duration = esp_timer_get_time() - start;

        int bit = (duration > 40) ? 1 : 0;
        data[i / 8] <<= 1;
        data[i / 8] |= bit;
    }

    // Checksum
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        return -2;
    }

    // Convert data
    *humidity = ((data[0] << 8) | data[1]) * 0.1f;

    int16_t raw_temp = (data[2] << 8) | data[3];
    if (raw_temp & 0x8000) {
        raw_temp &= 0x7FFF;
        *temperature = -raw_temp * 0.1f;
    } else {
        *temperature = raw_temp * 0.1f;
    }

    return 0;
}
```

---

## 9) Main Application

### `src/main.c`

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "dht.h"

#define DHT_PIN GPIO_NUM_4

void app_main(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DHT_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);

    while (1) {
        float temperature = 0;
        float humidity = 0;

        int res = dht_read(DHT_PIN, &temperature, &humidity);

        if (res == 0) {
            printf("Temperature: %.1f C, Humidity: %.1f %%\n", temperature, humidity);
        } else if (res == -2) {
            printf("Checksum error\n");
        } else {
            printf("Timeout error\n");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

---

## 10) Important Notes

### Timing sensitivity

- DHT22 is very timing-sensitive
- Use ESP-IDF timing functions (`esp_timer_get_time`, `esp_rom_delay_us`)

### Sampling rate

- Do not poll faster than **2 seconds**

### Pull-up resistor

- Required for stable communication

---

## 11) Troubleshooting

### Problem: Always timeout

- Check wiring
- Ensure pull-up resistor exists
- Verify correct GPIO pin

### Problem: Checksum error

- Electrical noise
- Timing instability
- Poor wiring

### Problem: Random values

- Reading too frequently
- Weak power supply

---

## 12) Practical Applications

- Weather station node
- Smart agriculture (greenhouse monitoring)
- HVAC monitoring
- IoT cloud sensor node (send to MQTT / GCP)

---

## 13) Summary

The DHT22 (AM2302) is a reliable digital temperature and humidity sensor.

Key takeaways:

- Uses single-wire timing protocol
- Requires pull-up resistor
- Needs careful timing on ESP32
- Best used with controlled sampling rate (≥2 sec)
- Works well in IoT and environmental monitoring systems

