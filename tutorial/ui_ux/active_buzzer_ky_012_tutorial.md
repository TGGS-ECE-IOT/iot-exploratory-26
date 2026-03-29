# KY-012 Active Buzzer Tutorial
## ESP32 + PlatformIO + ESP-IDF

---

## 1) Overview

The **KY-012 Active Buzzer module** is a simple sound output device that generates a tone when powered.

Key characteristics:

- Built-in oscillator (no PWM required)
- Produces fixed frequency tone
- Easy digital control (ON/OFF)

This makes it ideal for:

- Alerts and alarms
- Status indication
- Simple user feedback in IoT systems

---

## 2) Active vs Passive Buzzer

| Type | Behavior |
|------|---------|
| Active | Beeps when powered (no signal needed) |
| Passive | Requires PWM signal to generate tone |

KY-012 is an **active buzzer**, so:

- HIGH → sound ON
- LOW → sound OFF

---

## 3) Pinout

Typical KY-012 module pins:

| Pin | Function |
|-----|----------|
| S | Signal (control) |
| + | VCC |
| - | GND |

Some modules may label pins differently:

- S → SIG / OUT
- + → VCC
- - → GND

---

## 4) Wiring with ESP32

| KY-012 | ESP32 |
|--------|--------|
| S | GPIO23 |
| + | 3.3V |
| - | GND |

> KY-012 typically works with 3.3V on ESP32.

---

## 5) PlatformIO Setup

### platformio.ini

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
monitor_speed = 115200
```

---

## 6) Basic Control

### Example: Turn buzzer ON/OFF

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define BUZZER_PIN GPIO_NUM_23

void app_main(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);

    while (1) {
        printf("Buzzer ON\n");
        gpio_set_level(BUZZER_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));

        printf("Buzzer OFF\n");
        gpio_set_level(BUZZER_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## 7) Beep Pattern Example

### Short Beep

```c
void beep(int duration_ms) {
    gpio_set_level(BUZZER_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    gpio_set_level(BUZZER_PIN, 0);
}
```

---

### Double Beep

```c
void double_beep() {
    beep(200);
    vTaskDelay(pdMS_TO_TICKS(200));
    beep(200);
}
```

---

## 8) Alarm Pattern

```c
void alarm_pattern() {
    for (int i = 0; i < 5; i++) {
        gpio_set_level(BUZZER_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(300));
        gpio_set_level(BUZZER_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
```

---

## 9) Integration with Sensors

### Example: Obstacle Alert (HC-SR04)

```c
if (distance < 10) {
    gpio_set_level(BUZZER_PIN, 1);
} else {
    gpio_set_level(BUZZER_PIN, 0);
}
```

---

### Example: Air Quality Alert (MQ-135)

```c
if (air_quality_level == CRITICAL) {
    alarm_pattern();
}
```

---

## 10) Common Issues

### No sound

- Wrong pin connection
- GPIO not configured as output
- Wrong polarity

### Always ON

- Floating signal pin
- Incorrect wiring

### Very low volume

- Insufficient voltage
- Poor connection

---

## 11) Practical Applications

- Alarm system
- Notification sound
- IoT event alert
- Safety warning
- User interaction feedback

---

## 12) Summary

The KY-012 Active Buzzer is a simple but effective output device.

Key points:

- No PWM required
- Easy ON/OFF control
- Ideal for alerts and notifications
- Integrates well with sensor-based systems

It is highly suitable for your IoT workshop as an **audio feedback component**.

