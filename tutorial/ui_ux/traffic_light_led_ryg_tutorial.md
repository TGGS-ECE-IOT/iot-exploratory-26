# Traffic Light LED (RYG) Module Tutorial (ESP32 + PlatformIO + ESP-IDF)

## 1) Overview

The **Traffic Light LED module (RYG)** is a simple output module consisting of three LEDs:

- **R** = Red
- **Y** = Yellow
- **G** = Green

It is commonly used for:

- Status indication
- State machines
- Traffic light simulation
- Process visualization in IoT systems

---

## 2) Module Structure

The module typically includes:

- 3 LEDs (Red, Yellow, Green)
- Current-limiting resistors (already onboard)
- 4-pin header

### Pin labels (typical)

| Pin | Function |
|---|---|
| GND | Ground |
| R | Red LED control |
| Y | Yellow LED control |
| G | Green LED control |

---

## 3) Working Principle

Each LED is controlled independently via a GPIO pin.

- Set GPIO HIGH → LED ON
- Set GPIO LOW → LED OFF

> Most modules are **active HIGH**, but always verify if your module behaves differently.

---

## 4) Wiring with ESP32

Example connection:

| Module Pin | ESP32 |
|---|---|
| GND | GND |
| R | GPIO25 |
| Y | GPIO26 |
| G | GPIO27 |

You can use any available GPIO pins.

---

## 5) PlatformIO Setup

### 5.1 Folder structure

```text
traffic_light_esp32/
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

## 6) Basic LED Control

### `src/main.c`

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED_RED    GPIO_NUM_25
#define LED_YELLOW GPIO_NUM_26
#define LED_GREEN  GPIO_NUM_27

void app_main(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_RED) | (1ULL << LED_YELLOW) | (1ULL << LED_GREEN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);

    while (1) {
        // Red ON
        gpio_set_level(LED_RED, 1);
        gpio_set_level(LED_YELLOW, 0);
        gpio_set_level(LED_GREEN, 0);
        printf("RED\n");
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Yellow ON
        gpio_set_level(LED_RED, 0);
        gpio_set_level(LED_YELLOW, 1);
        gpio_set_level(LED_GREEN, 0);
        printf("YELLOW\n");
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Green ON
        gpio_set_level(LED_RED, 0);
        gpio_set_level(LED_YELLOW, 0);
        gpio_set_level(LED_GREEN, 1);
        printf("GREEN\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

---

## 7) Traffic Light State Machine

A real traffic light follows a sequence:

1. Green → Go
2. Yellow → Prepare to stop
3. Red → Stop

### Example timing

| State | Duration |
|---|---|
| Green | 5 seconds |
| Yellow | 2 seconds |
| Red | 5 seconds |

---

### Improved implementation

```c
typedef enum {
    STATE_RED,
    STATE_GREEN,
    STATE_YELLOW
} traffic_state_t;

void set_state(traffic_state_t state) {
    gpio_set_level(LED_RED, state == STATE_RED);
    gpio_set_level(LED_YELLOW, state == STATE_YELLOW);
    gpio_set_level(LED_GREEN, state == STATE_GREEN);
}

void app_main(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_RED) | (1ULL << LED_YELLOW) | (1ULL << LED_GREEN),
        .mode = GPIO_MODE_OUTPUT
    };

    gpio_config(&io_conf);

    while (1) {
        set_state(STATE_GREEN);
        printf("GREEN\n");
        vTaskDelay(pdMS_TO_TICKS(5000));

        set_state(STATE_YELLOW);
        printf("YELLOW\n");
        vTaskDelay(pdMS_TO_TICKS(2000));

        set_state(STATE_RED);
        printf("RED\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

---

## 8) Blinking and Alerts

### Example: Red blinking alert

```c
void blink_red(int times) {
    for (int i = 0; i < times; i++) {
        gpio_set_level(LED_RED, 1);
        vTaskDelay(pdMS_TO_TICKS(300));
        gpio_set_level(LED_RED, 0);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
```

---

## 9) Integration with Sensors (IoT Node)

Example logic for your workshop:

- Green → Normal condition
- Yellow → Warning (e.g., air quality rising)
- Red → Critical condition (e.g., obstacle detected or gas spike)

Example mapping:

```text
MQ-135 normal → GREEN
MQ-135 moderate → YELLOW
MQ-135 high → RED
```

Or:

```text
HC-SR04 distance < 10 cm → RED
10–30 cm → YELLOW
> 30 cm → GREEN
```

---

## 10) Common Issues

### LED not lighting

- Wrong wiring
- Incorrect GPIO number
- Module requires active LOW (rare)

### LEDs always ON

- Floating pins
- Incorrect GPIO configuration

### Brightness differences

- Normal due to LED color characteristics

---

## 11) Practical Applications

- Traffic light simulation
- Industrial process indicator
- Machine state visualization
- Alarm system
- IoT status indicator

---

## 12) Summary

The Traffic Light LED (RYG) module is a simple but powerful visual indicator.

Key points:

- Easy GPIO-based control
- Useful for state representation
- Ideal for combining with sensors
- Perfect for teaching state machines and embedded control

It is highly recommended for your workshop as a **visual feedback component**.

