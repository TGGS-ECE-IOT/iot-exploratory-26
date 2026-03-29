# HC-SR04 Ultrasonic Module Tutorial

## 1) What is the HC-SR04?

The **HC-SR04** is a low-cost **ultrasonic distance sensor** used to measure the distance from the sensor to an object.

It works by:

1. Sending out an ultrasonic sound pulse.
2. Waiting for the echo to bounce back from an object.
3. Measuring the travel time of that echo.
4. Converting the travel time into distance.

This makes it useful for:

- Distance measurement
- Obstacle detection
- Water level estimation
- Robot navigation
- Simple automation projects

---

## 2) Main Features

Typical HC-SR04 specifications:

- Operating voltage: **5 V**
- Working current: about **15 mA**
- Measuring range: about **2 cm to 400 cm**
- Measuring angle: about **15°**
- Resolution: about **3 mm**
- Interface: **Trigger** and **Echo** digital pins

> Note: The exact performance depends on target shape, surface material, angle, and environmental conditions.

---

## 3) Pinout

The HC-SR04 has 4 pins:

| Pin | Function |
|---|---|
| VCC | Power supply, usually 5 V |
| Trig | Trigger input |
| Echo | Echo output |
| GND | Ground |

### How the pins work

- **Trig**: The microcontroller sends a short pulse to start measurement.
- **Echo**: The sensor returns a pulse whose width represents the round-trip travel time of the ultrasonic wave.

---

## 4) Working Principle

The measurement sequence is:

1. Keep **Trig** low briefly.
2. Send a **10 microsecond HIGH pulse** to **Trig**.
3. The sensor emits an ultrasonic burst at about **40 kHz**.
4. The sensor sets **Echo** HIGH until the reflected signal returns.
5. Measure how long **Echo** stays HIGH.

### Distance formula

Because the sound travels to the object and back:

```text
distance = (time × speed of sound) / 2
```

Using the common approximation:

- Speed of sound ≈ **343 m/s** = **0.0343 cm/µs**

So:

```text
distance_cm = (echo_time_us × 0.0343) / 2
```

A widely used simplified formula is:

```text
distance_cm = echo_time_us / 58.0
```

---

## 5) Wiring Example

### Example with a 5 V Arduino-compatible board

| HC-SR04 | Microcontroller |
|---|---|
| VCC | 5 V |
| GND | GND |
| Trig | Digital output pin |
| Echo | Digital input pin |

### Important note for ESP32

The **HC-SR04 Echo pin outputs 5 V**, but **ESP32 GPIO pins are 3.3 V only**.

So when using ESP32:

- You should use a **voltage divider** or a **logic level shifter** on the **Echo** pin.
- Trig from ESP32 to HC-SR04 is usually fine because 3.3 V HIGH is normally recognized by the sensor.

A simple resistor divider example for **Echo -> ESP32 GPIO**:

- R1 = 1 kΩ
- R2 = 2 kΩ

This reduces the 5 V Echo signal to a safer voltage for ESP32.

---

## 6) Basic Arduino Example Code

```cpp
const int trigPin = 9;
const int echoPin = 10;

void setup() {
  Serial.begin(9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void loop() {
  long duration;
  float distanceCm;

  // Ensure trigger is LOW
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Send 10 us pulse to trigger
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read echo pulse width in microseconds
  duration = pulseIn(echoPin, HIGH);

  // Convert to distance in cm
  distanceCm = duration / 58.0;

  Serial.print("Distance: ");
  Serial.print(distanceCm);
  Serial.println(" cm");

  delay(500);
}
```

### Code explanation

- `pulseIn(echoPin, HIGH)` measures how long the Echo pin stays HIGH.
- Dividing by `58.0` converts microseconds to centimeters.
- The result is printed to the serial monitor every 500 ms.

---

## 7) ESP32 Example Code (PlatformIO + ESP-IDF)

This example uses:

- **ESP32**
- **PlatformIO**
- **ESP-IDF framework**
- **C language**

> Important: The HC-SR04 Echo pin is a 5 V output. Do not connect it directly to an ESP32 GPIO. Use a voltage divider or logic level shifter.

### 7.1 Project structure

A typical PlatformIO project looks like this:

```text
hc_sr04_esp32/
├── include/
├── lib/
├── src/
│   └── main.c
└── platformio.ini
```

### 7.2 `platformio.ini`

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
monitor_speed = 115200
```

### 7.3 `src/main.c`

```c
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#define TRIG_PIN GPIO_NUM_5
#define ECHO_PIN GPIO_NUM_18

static float hc_sr04_read_distance_cm(void) {
    // Ensure trigger is low
    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(2);

    // Send 10 us trigger pulse
    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    // Wait for echo to go high
    int64_t timeout_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 0) {
        if ((esp_timer_get_time() - timeout_start) > 30000) {
            return -1.0f;
        }
    }

    // Measure how long echo stays high
    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 1) {
        if ((esp_timer_get_time() - echo_start) > 30000) {
            return -1.0f;
        }
    }
    int64_t echo_end = esp_timer_get_time();

    int64_t duration_us = echo_end - echo_start;
    return (float)duration_us / 58.0f;
}

void app_main(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TRIG_PIN) | (1ULL << ECHO_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    // Configure Echo as input first
    gpio_config(&io_conf);

    // Configure Trig as output
    gpio_set_direction(TRIG_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(TRIG_PIN, 0);

    while (1) {
        float distance_cm = hc_sr04_read_distance_cm();

        if (distance_cm < 0) {
            printf("No echo received
");
        } else {
            printf("Distance: %.2f cm
", distance_cm);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

### 7.4 How the code works

- `esp_rom_delay_us()` is used to generate the short trigger pulse.
- `esp_timer_get_time()` returns time in microseconds.
- The code waits for **Echo** to go HIGH, then measures how long it stays HIGH.
- A timeout is added so the program does not get stuck forever if no echo is received.
- Distance is converted using:

```text
distance_cm = duration_us / 58.0
```

---

## 8) Improving Stability

Ultrasonic readings can fluctuate. Common improvements:

### 8.1 Average multiple readings

Take 3 to 10 readings and average them.

### 8.2 Ignore invalid values

Reject:

- Zero readings caused by timeout
- Values outside the expected range
- Sudden unrealistic jumps

### 8.3 Keep the sensor aimed properly

Best results happen when:

- The object is in front of the sensor
- The object surface reflects sound well
- The sensor is approximately perpendicular to the object

---

## 9) Example: Averaging Multiple Readings (PlatformIO + ESP-IDF)

```c
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#define TRIG_PIN GPIO_NUM_5
#define ECHO_PIN GPIO_NUM_18

static float hc_sr04_read_distance_cm_once(void) {
    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(2);

    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    int64_t timeout_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 0) {
        if ((esp_timer_get_time() - timeout_start) > 30000) {
            return -1.0f;
        }
    }

    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 1) {
        if ((esp_timer_get_time() - echo_start) > 30000) {
            return -1.0f;
        }
    }
    int64_t echo_end = esp_timer_get_time();

    int64_t duration_us = echo_end - echo_start;
    return (float)duration_us / 58.0f;
}

static float hc_sr04_read_distance_cm_average(int samples) {
    float sum = 0.0f;
    int valid_count = 0;

    for (int i = 0; i < samples; i++) {
        float d = hc_sr04_read_distance_cm_once();
        if (d > 0) {
            sum += d;
            valid_count++;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (valid_count == 0) {
        return -1.0f;
    }

    return sum / valid_count;
}

void app_main(void) {
    gpio_config_t echo_conf = {
        .pin_bit_mask = (1ULL << ECHO_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&echo_conf);

    gpio_config_t trig_conf = {
        .pin_bit_mask = (1ULL << TRIG_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&trig_conf);

    gpio_set_level(TRIG_PIN, 0);

    while (1) {
        float distance_cm = hc_sr04_read_distance_cm_average(5);

        if (distance_cm < 0) {
            printf("No valid reading
");
        } else {
            printf("Average distance: %.2f cm
", distance_cm);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

---

## 10) Common Problems and Troubleshooting

### Problem 1: Always reads 0 or timeout

Possible causes:

- Wrong wiring
- No common ground
- Echo pin not connected correctly
- Object is too far away
- Object surface does not reflect ultrasound well

### Problem 2: Readings are unstable

Possible causes:

- Sensor vibrating or moving
- Irregular object surface
- Interference from other ultrasonic sensors
- Electrical noise

### Problem 3: ESP32 input damage risk

Cause:

- Echo pin connected directly to ESP32 without voltage reduction

Fix:

- Use a voltage divider or logic level shifter

---

## 11) Practical Usage Ideas

The HC-SR04 can be used in many beginner and intermediate projects:

- Tank or water level estimation
- Parking assist indicator
- Robot obstacle avoidance
- Smart trash bin lid opening
- Presence or distance-triggered buzzer/alarm
- Cloud IoT demo with measured distance sent by ESP32 via MQTT

---

## 12) Example Mini Application Logic

### Obstacle warning with buzzer (PlatformIO + ESP-IDF)

You can define a threshold such as:

- Distance < 10 cm -> buzzer ON
- Distance >= 10 cm -> buzzer OFF

Example code:

```c
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#define TRIG_PIN   GPIO_NUM_5
#define ECHO_PIN   GPIO_NUM_18
#define BUZZER_PIN GPIO_NUM_19

static float hc_sr04_read_distance_cm(void) {
    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(2);
    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    int64_t timeout_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 0) {
        if ((esp_timer_get_time() - timeout_start) > 30000) {
            return -1.0f;
        }
    }

    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 1) {
        if ((esp_timer_get_time() - echo_start) > 30000) {
            return -1.0f;
        }
    }
    int64_t echo_end = esp_timer_get_time();

    int64_t duration_us = echo_end - echo_start;
    return (float)duration_us / 58.0f;
}

void app_main(void) {
    gpio_config_t input_conf = {
        .pin_bit_mask = (1ULL << ECHO_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&input_conf);

    gpio_config_t output_conf = {
        .pin_bit_mask = (1ULL << TRIG_PIN) | (1ULL << BUZZER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&output_conf);

    gpio_set_level(TRIG_PIN, 0);
    gpio_set_level(BUZZER_PIN, 0);

    while (1) {
        float distance_cm = hc_sr04_read_distance_cm();

        if (distance_cm > 0 && distance_cm < 10.0f) {
            gpio_set_level(BUZZER_PIN, 1);
        } else {
            gpio_set_level(BUZZER_PIN, 0);
        }

        if (distance_cm < 0) {
            printf("No echo received
");
        } else {
            printf("Distance: %.2f cm
", distance_cm);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

---

## 13) Summary

The HC-SR04 is a simple and useful ultrasonic sensor for distance measurement.

Key points:

- It measures distance using ultrasonic echo timing.
- It needs a **10 µs trigger pulse**.
- The **Echo pulse width** is converted into distance.
- On **ESP32**, protect the Echo input because it is a **5 V output**.
- Averaging multiple readings improves stability.
- In **PlatformIO with ESP-IDF**, timing can be implemented using `esp_timer_get_time()` and `esp_rom_delay_us()`.

For workshop use, the HC-SR04 is a very good sensor to demonstrate:

- Basic GPIO input/output
- Microsecond timing measurement
- Physical sensing
- Local UI/alert logic
- Cloud-connected sensor reporting

