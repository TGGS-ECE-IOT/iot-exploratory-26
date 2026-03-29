#include <math.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "esp_adc/adc_oneshot.h"

#include "esp_err.h"
#include "rom/ets_sys.h"

#include "app_state.h"
#include "pin_config.h"
#include "sensors.h"
#include "utils.h"

static adc_oneshot_unit_handle_t s_adc1_handle;

static int dht_expect_level(int level, int timeout_us) {
    int t = 0;
    while (gpio_get_level(PIN_DHT22) == level) {
        if (++t > timeout_us) return -1;
        ets_delay_us(1);
    }
    return t;
}

static esp_err_t dht22_read(float *temperature_c, float *humidity_pct) {
    uint8_t data[5] = {0};

    gpio_set_direction(PIN_DHT22, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_DHT22, 0);
    ets_delay_us(1200);
    gpio_set_level(PIN_DHT22, 1);
    ets_delay_us(30);
    gpio_set_direction(PIN_DHT22, GPIO_MODE_INPUT);

    if (dht_expect_level(1, 80) < 0) return ESP_FAIL;
    if (dht_expect_level(0, 100) < 0) return ESP_FAIL;
    if (dht_expect_level(1, 100) < 0) return ESP_FAIL;

    for (int i = 0; i < 40; i++) {
        if (dht_expect_level(0, 70) < 0) return ESP_FAIL;
        int high_time = dht_expect_level(1, 120);
        if (high_time < 0) return ESP_FAIL;
        data[i / 8] <<= 1;
        if (high_time > 40) data[i / 8] |= 1;
    }

    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) return ESP_ERR_INVALID_CRC;

    uint16_t raw_h = (data[0] << 8) | data[1];
    uint16_t raw_t = (data[2] << 8) | data[3];

    *humidity_pct = raw_h / 10.0f;
    if (raw_t & 0x8000) {
        raw_t &= 0x7FFF;
        *temperature_c = -raw_t / 10.0f;
    } else {
        *temperature_c = raw_t / 10.0f;
    }

    return ESP_OK;
}

static float hcsr04_read_cm(void) {
    gpio_set_level(PIN_HCSR04_TRIG, 0);
    ets_delay_us(3);
    gpio_set_level(PIN_HCSR04_TRIG, 1);
    ets_delay_us(10);
    gpio_set_level(PIN_HCSR04_TRIG, 0);

    int timeout = 30000;
    while (gpio_get_level(PIN_HCSR04_ECHO) == 0 && timeout-- > 0) ets_delay_us(1);
    if (timeout <= 0) return -1.0f;

    int pulse_us = 0;
    while (gpio_get_level(PIN_HCSR04_ECHO) == 1 && pulse_us < 30000) {
        pulse_us++;
        ets_delay_us(1);
    }

    if (pulse_us <= 0) return -1.0f;
    return pulse_us / 58.0f;
}

static int avg_adc(adc_channel_t ch, int samples) {
    long sum = 0;
    for (int i = 0; i < samples; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc1_handle, ch, &raw) == ESP_OK) {
            sum += raw;
        }
        ets_delay_us(500);
    }
    return (int)(sum / samples);
}

static void update_led_status(const sensor_data_t *s) {
    if (g_song_playing) {
        return;
    }
    gpio_set_level(PIN_LED_RED, (s->mq135_pct > 75.0f) || (s->temperature_c > 35.0f));
    gpio_set_level(PIN_LED_YELLOW, (s->distance_cm > 0 && s->distance_cm < 20.0f));
    gpio_set_level(PIN_LED_GREEN, wifi_connected ? 1 : 0);
}

void sensors_init_hardware(void) {
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << PIN_LED_RED) | (1ULL << PIN_LED_YELLOW) | (1ULL << PIN_LED_GREEN) | (1ULL << PIN_HCSR04_TRIG),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out_cfg));

    gpio_set_level(PIN_LED_RED, 0);
    gpio_set_level(PIN_LED_YELLOW, 0);
    gpio_set_level(PIN_LED_GREEN, 0);
    gpio_set_level(PIN_HCSR04_TRIG, 0);

    gpio_config_t sensor_in_cfg = {
        .pin_bit_mask = (1ULL << PIN_HCSR04_ECHO) | (1ULL << PIN_PIR) | (1ULL << PIN_DHT22),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&sensor_in_cfg));

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc1_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, (adc_channel_t)PIN_MQ135_ADC, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, (adc_channel_t)PIN_LDR_ADC, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, (adc_channel_t)PIN_POT_ADC, &chan_cfg));
}

void sensor_task(void *arg) {
    const int adc_samples = 16;
    while (1) {
        float temp_sum = 0.0f;
        float hum_sum = 0.0f;
        float dist_sum = 0.0f;
        int temp_ok = 0;
        int dist_ok = 0;

        for (int i = 0; i < 4; i++) {
            float t = 0.0f;
            float h = 0.0f;
            if (dht22_read(&t, &h) == ESP_OK) {
                temp_sum += t;
                hum_sum += h;
                temp_ok++;
            }

            float d = hcsr04_read_cm();
            if (d > 0.0f && d < 400.0f) {
                dist_sum += d;
                dist_ok++;
            }

            vTaskDelay(pdMS_TO_TICKS(60));
        }

        sensor_data_t s = {0};
        s.temperature_c = (temp_ok > 0) ? temp_sum / temp_ok : NAN;
        s.humidity_pct = (temp_ok > 0) ? hum_sum / temp_ok : NAN;
        s.distance_cm = (dist_ok > 0) ? dist_sum / dist_ok : -1.0f;
        s.pir_motion = gpio_get_level(PIN_PIR);
        s.mq135_raw = avg_adc((adc_channel_t)PIN_MQ135_ADC, adc_samples);
        s.ldr_raw = avg_adc((adc_channel_t)PIN_LDR_ADC, adc_samples);
        s.pot_raw = avg_adc((adc_channel_t)PIN_POT_ADC, adc_samples);
        s.mq135_pct = raw_to_pct(s.mq135_raw);
        s.ldr_pct = raw_to_pct(s.ldr_raw);
        s.pot_pct = raw_to_pct(s.pot_raw);
        s.uptime_ms = now_ms();
        get_iso_time(s.iso_time, sizeof(s.iso_time));

        xSemaphoreTake(data_mutex, portMAX_DELAY);
        g_sensor = s;
        xSemaphoreGive(data_mutex);

        update_led_status(&s);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
