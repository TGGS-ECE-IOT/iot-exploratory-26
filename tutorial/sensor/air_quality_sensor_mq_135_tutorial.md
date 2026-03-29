# MQ-135 Air Quality Sensor Module Tutorial (ESP32 + PlatformIO + ESP-IDF)

## 1) What is the MQ-135?

The **MQ-135** is a low-cost **gas and air quality sensor** often used in hobby and educational projects to detect changes in air composition.

It is commonly described as being sensitive to gases such as:

- Ammonia (NH3)
- Nitrogen oxides (NOx)
- Alcohol vapor
- Benzene
- Smoke
- Carbon dioxide related air-quality changes

In practice, the MQ-135 is best used as a **general air quality trend sensor**, not as a precision gas analyzer.

---

## 2) Important Limitation

The MQ-135 is **not a true selective gas sensor**.

That means:

- It responds to multiple gases
- The reading depends on temperature and humidity
- It needs calibration
- Different modules may behave differently
- The analog output is more useful for **relative change** than exact ppm

So for workshop use, treat it as:

- **Good for comparing cleaner vs dirtier air**
- **Good for detecting sudden air quality changes**
- **Not good enough for certified measurement**

---

## 3) Module Overview

Most MQ-135 modules include:

- MQ-135 gas sensor can
- Comparator IC
- Sensitivity adjustment potentiometer
- Power LED
- Digital output LED
- Analog output pin
- Digital output pin

Typical pins on the module:

| Pin | Function |
|---|---|
| VCC | Power supply |
| GND | Ground |
| AO | Analog output |
| DO | Digital output from comparator |

---

## 4) AO vs DO

### Analog output (AO)

The **AO** pin gives a continuous analog voltage related to the sensor resistance.

Use AO when you want:

- Raw measurement value
- Trend analysis
- Cloud logging
- Threshold logic in software

### Digital output (DO)

The **DO** pin is produced by the onboard comparator.

Use DO when you want:

- Simple threshold detection
- Binary trigger such as alarm / no alarm

For ESP32 IoT work, **AO is usually the more useful output**.

---

## 5) Power and Voltage Caution with ESP32

Many MQ-135 modules are designed for **5 V supply**, and their analog output can approach a voltage that may be **too high for ESP32 ADC pins**.

ESP32 GPIO pins are **3.3 V only**.

### Important safety note

Before connecting MQ-135 AO to ESP32:

- Check the module output range
- Use a **voltage divider** if needed
- Do not assume AO is always safe at 3.3 V

### Safer approaches

1. Power the module according to the module specification and scale the AO voltage down before feeding ESP32 ADC
2. Use a dedicated ADC interface circuit
3. Use DO only for simple threshold experiments

For workshop purposes, the safest common approach is:

- Use the MQ-135 module power as recommended by its module design
- Read **AO through a voltage divider** into ESP32 ADC

---

## 6) Heating and Warm-Up

The MQ-135 contains an internal heater.

This means:

- It consumes more power than simple digital sensors
- It needs **warm-up time** before readings stabilize
- Readings drift during the first minutes of operation

### Practical warm-up guideline

- Quick demo: wait **1 to 5 minutes**
- Better stability: wait **10 to 20 minutes**
- First-time baseline characterization can take even longer

For workshop tutorials, it is acceptable to:

- power the module early
- explain that early values are only approximate

---

## 7) Wiring with ESP32

### Recommended analog wiring

| MQ-135 Module | ESP32 |
|---|---|
| VCC | Module-required supply |
| GND | GND |
| AO | ESP32 ADC pin through voltage divider |

Example:

- AO -> voltage divider -> GPIO34

### Optional digital wiring

| MQ-135 Module | ESP32 |
|---|---|
| DO | Any digital input pin |

Example:

- DO -> GPIO27

> GPIO34 is input-only on many ESP32 boards, which is suitable for ADC input.

---

## 8) ESP32 ADC Notes

ESP32 ADC readings are not perfectly linear and can vary between boards.

So in most workshop use cases:

- use ADC values mainly for comparison and trend analysis
- do not over-interpret them as precise concentration values

A typical workflow is:

1. read raw ADC
2. convert to approximate voltage
3. compare against a baseline
4. trigger alerts when air quality changes significantly

---

## 9) PlatformIO Project Setup

### 9.1 Folder structure

```text
mq135_esp32/
├── include/
├── lib/
├── src/
│   └── main.c
└── platformio.ini
```

### 9.2 `platformio.ini`

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
monitor_speed = 115200
```

---

## 10) Basic Analog Reading Example

This example reads the **AO** pin using ESP32 ADC in **ESP-IDF**.

### `src/main.c`

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#define MQ135_ADC_CHANNEL ADC_CHANNEL_6   // GPIO34 on many ESP32 boards
#define MQ135_ADC_UNIT    ADC_UNIT_1

void app_main(void) {
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = MQ135_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, MQ135_ADC_CHANNEL, &config));

    while (1) {
        int adc_raw = 0;
        esp_err_t err = adc_oneshot_read(adc_handle, MQ135_ADC_CHANNEL, &adc_raw);

        if (err == ESP_OK) {
            printf("MQ-135 raw ADC: %d\n", adc_raw);
        } else {
            printf("ADC read error\n");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## 11) Approximate Voltage Conversion

Sometimes it is useful to display approximate input voltage.

Because ESP32 ADC calibration varies by chip and attenuation setting, a simple approximate conversion may be enough for demos.

Example concept:

```text
voltage ≈ raw / 4095 × Vref_equivalent
```

However, the real conversion depends on:

- ADC attenuation
- ADC reference variation
- board and ESP32 chip characteristics

For workshop dashboards, **raw ADC + baseline comparison** is usually sufficient.

---

## 12) Baseline-Based Air Quality Trend Example

A practical approach is to:

- measure a baseline value in normal air
- compare future readings against that baseline
- use the difference as a simple air quality indicator

### Example logic

- close to baseline -> normal
- moderately above baseline -> degraded air quality
- far above baseline -> poor air quality or gas exposure event

### `src/main.c` example with simple trend logic

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#define MQ135_ADC_CHANNEL ADC_CHANNEL_6   // GPIO34
#define MQ135_ADC_UNIT    ADC_UNIT_1

static int read_average(adc_oneshot_unit_handle_t adc_handle, int samples) {
    int sum = 0;
    int count = 0;

    for (int i = 0; i < samples; i++) {
        int raw = 0;
        if (adc_oneshot_read(adc_handle, MQ135_ADC_CHANNEL, &raw) == ESP_OK) {
            sum += raw;
            count++;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (count == 0) {
        return -1;
    }

    return sum / count;
}

void app_main(void) {
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = MQ135_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, MQ135_ADC_CHANNEL, &config));

    printf("Warming up MQ-135...\n");
    vTaskDelay(pdMS_TO_TICKS(10000));

    int baseline = read_average(adc_handle, 20);
    printf("Baseline ADC: %d\n", baseline);

    while (1) {
        int value = read_average(adc_handle, 10);

        if (value < 0) {
            printf("ADC read failed\n");
        } else {
            int delta = value - baseline;

            printf("MQ-135 value: %d, delta: %d -> ", value, delta);

            if (delta < 100) {
                printf("Air normal\n");
            } else if (delta < 300) {
                printf("Air quality degraded\n");
            } else {
                printf("Air quality poor / strong gas presence\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

---

## 13) Using the DO Pin for Threshold Detection

If you want simple digital detection, the onboard potentiometer can adjust the comparator threshold.

### Example use case

- DO = LOW or HIGH when gas level crosses threshold
- trigger buzzer, LED, or cloud alert

### Example digital input code

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define MQ135_DO_PIN GPIO_NUM_27

void app_main(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MQ135_DO_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    while (1) {
        int state = gpio_get_level(MQ135_DO_PIN);
        printf("MQ-135 DO state: %d\n", state);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

---

## 14) Toward Approximate ppm Estimation

You will often see Internet examples converting MQ-135 readings into **ppm** values.

Be careful.

To estimate ppm meaningfully, you usually need:

- load resistance information
- sensor resistance calculation
- baseline resistance in clean air (`R0`)
- calibration data
- gas-specific sensitivity curve
- compensation for environment

Without proper calibration, ppm values can be misleading.

For a workshop tutorial, it is better to use:

- raw ADC
- normalized score
- baseline-relative change
- warning levels

rather than claiming exact ppm.

---

## 15) Troubleshooting

### Problem: Reading is unstable

Possible causes:

- insufficient warm-up time
- noisy power supply
- sensor not yet stabilized
- air flow changes
- temperature and humidity changes

### Problem: Reading always near maximum

Possible causes:

- AO voltage too high
- wrong ADC configuration
- incorrect wiring
- unsuitable module/output scaling

### Problem: No digital threshold response on DO

Possible causes:

- comparator threshold not adjusted
- gas concentration below threshold
- wiring error

### Problem: ESP32 pin damage risk

Cause:

- analog output or digital output exceeding 3.3 V input tolerance

Fix:

- verify the module output voltage
- use a voltage divider or proper level shifting where needed

---

## 16) Practical Applications

The MQ-135 is useful in workshop and prototype projects such as:

- classroom air quality trend monitoring
- smoke or odor change detection
- indoor ventilation demonstrations
- greenhouse environment trend monitoring
- IoT cloud dashboards showing air quality change
- sensor fusion with temperature/humidity and occupancy data

---

## 17) Example IoT Usage Idea

In your ESP32 sensor node, MQ-135 can be combined with:

- DHT22 for temperature and humidity
- PIR for occupancy
- buzzer / RGB LED for local alert
- OLED screen for local display
- MQTT for cloud upload

Example message payload:

```json
{
  "device_id": "sensor_node_01",
  "mq135_raw": 1480,
  "mq135_delta": 210,
  "air_quality_status": "degraded"
}
```

This is often more useful in practice than trying to publish uncertain ppm values.

---

## 18) Summary

The MQ-135 is a practical low-cost sensor for **general air quality trend monitoring**.

Key points:

- It is best used as a **relative air quality sensor**
- It is not highly selective for a single gas
- It needs warm-up and calibration
- AO is best for trend logging and cloud dashboards
- DO is useful for simple threshold alarms
- Be careful with **5 V module output vs 3.3 V ESP32 input limits**
- For workshop use, focus on **baseline, trend, and alert logic** rather than exact ppm claims

