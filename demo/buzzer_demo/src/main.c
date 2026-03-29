#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "BUZZER_DEMO";

/* ---------------- Pin Assignment ---------------- */
#define PIN_BUZZER      GPIO_NUM_23
#define PIN_LED_RED     GPIO_NUM_25
#define PIN_LED_YELLOW  GPIO_NUM_26
#define PIN_LED_GREEN   GPIO_NUM_27
#define PIN_BTN_RED     GPIO_NUM_16
#define PIN_BTN_BLUE    GPIO_NUM_17
#define PIN_SLIDER      ADC_CHANNEL_7   // GPIO35 = ADC1_CH7 on ESP32

/* ---------------- ADC Config ---------------- */
#define ADC_UNIT_USED   ADC_UNIT_1
#define ADC_ATTEN_USED  ADC_ATTEN_DB_12

/* ---------------- Timing ---------------- */
#define MAIN_LOOP_MS            10
#define BUTTON_DEBOUNCE_MS      50
#define SONG_GAP_MS             250

/* 
 * Tempo mapping:
 * Smaller scale = faster song
 * Larger scale  = slower song
 */
#define TEMPO_MIN_PERCENT       40   // fastest
#define TEMPO_MAX_PERCENT       180  // slowest

/* ---------------- Song Step ---------------- */
typedef struct {
    uint16_t units;   // duration in "ticks"
    bool buzzer_on;   // true = beep, false = silence
} song_step_t;

/*
 * Each song is only rhythm because active buzzer can play one tone only.
 * units are scaled by tempo and a base tick time.
 */
#define END_SONG {0, false}

/*
 * Unit guide with base_tick_ms = 100:
 * 1 = 100 ms
 * 2 = 200 ms
 * 3 = 300 ms
 * 4 = 400 ms
 */

/* Song 1: "We Will Rock You" rhythm
 * beep(100); delay(100);
 * beep(100); delay(100);
 * beep(300); delay(400);
 */
static const song_step_t song1[] = {
    {1, true}, {1, false},
    {1, true}, {1, false},
    {3, true}, {4, false},

    {1, true}, {1, false},
    {1, true}, {1, false},
    {3, true}, {4, false},
    END_SONG
};

/* Song 2: "Happy Birthday" rhythm only
 * short short long
 * short short long
 * short long long
 */
static const song_step_t song2[] = {
    {1, true}, {1, false},
    {1, true}, {2, false},
    {3, true}, {3, false},

    {1, true}, {1, false},
    {1, true}, {2, false},
    {3, true}, {3, false},

    {1, true}, {2, false},
    {3, true}, {2, false},
    {3, true}, {4, false},
    END_SONG
};

/* Song 3: "Jingle Bells" rhythm only
 * short short short
 * long
 * short short short
 * long
 */
static const song_step_t song3[] = {
    {1, true}, {1, false},
    {1, true}, {1, false},
    {1, true}, {1, false},
    {3, true}, {2, false},

    {1, true}, {1, false},
    {1, true}, {1, false},
    {1, true}, {1, false},
    {3, true}, {3, false},
    END_SONG
};

// /* Song 1: March-like */
// static const song_step_t song1[] = {
//     {2, true}, {1, false},
//     {2, true}, {1, false},
//     {4, true}, {2, false},
//     {2, true}, {1, false},
//     {2, true}, {1, false},
//     {6, true}, {4, false},

//     {2, true}, {1, false},
//     {2, true}, {1, false},
//     {4, true}, {2, false},
//     {2, true}, {1, false},
//     {2, true}, {1, false},
//     {8, true}, {4, false},
//     END_SONG
// };

// /* Song 2: Dance-like syncopation */
// static const song_step_t song2[] = {
//     {1, true}, {1, false},
//     {1, true}, {1, false},
//     {2, true}, {1, false},
//     {1, true}, {1, false},
//     {3, true}, {2, false},

//     {1, true}, {1, false},
//     {1, true}, {1, false},
//     {2, true}, {1, false},
//     {1, true}, {1, false},
//     {4, true}, {3, false},

//     {2, true}, {1, false},
//     {1, true}, {1, false},
//     {1, true}, {1, false},
//     {5, true}, {4, false},
//     END_SONG
// };

// /* Song 3: Alarm / game style */
// static const song_step_t song3[] = {
//     {1, true}, {1, false},
//     {1, true}, {1, false},
//     {1, true}, {1, false},
//     {1, true}, {2, false},

//     {3, true}, {1, false},
//     {3, true}, {1, false},
//     {2, true}, {2, false},

//     {1, true}, {1, false},
//     {1, true}, {1, false},
//     {4, true}, {4, false},
//     END_SONG
// };

static const song_step_t *songs[] = {song1, song2, song3};
static const char *song_names[] = {"March Beat", "Dance Beat", "Game Alarm"};
static const int song_count = sizeof(songs) / sizeof(songs[0]);

/* ---------------- Globals ---------------- */
static adc_oneshot_unit_handle_t adc_handle;

typedef struct {
    int current_song;
    int step_index;
    bool song_finished;
    int64_t step_deadline_ms;
    int led_phase;
} player_t;

typedef struct {
    bool stable_state;
    bool last_raw;
    int64_t last_change_ms;
} button_t;

static player_t player = {
    .current_song = 0,
    .step_index = 0,
    .song_finished = false,
    .step_deadline_ms = 0,
    .led_phase = 0
};

static button_t btn_red = {true, true, 0};   // pull-up button: idle = HIGH
static button_t btn_blue = {true, true, 0};

/* ---------------- Utility ---------------- */
static int64_t now_ms(void) {
    return esp_timer_get_time() / 1000;
}

static void set_leds(bool r, bool y, bool g) {
    gpio_set_level(PIN_LED_RED, r ? 1 : 0);
    gpio_set_level(PIN_LED_YELLOW, y ? 1 : 0);
    gpio_set_level(PIN_LED_GREEN, g ? 1 : 0);
}

static void all_off(void) {
    gpio_set_level(PIN_BUZZER, 0);
    set_leds(false, false, false);
}

static void set_buzzer(bool on) {
    gpio_set_level(PIN_BUZZER, on ? 1 : 0);
}

static void flash_led_pattern(int phase) {
    switch (phase % 4) {
        case 0:
            set_leds(true, false, false);
            break;
        case 1:
            set_leds(false, true, false);
            break;
        case 2:
            set_leds(false, false, true);
            break;
        default:
            set_leds(true, true, true);
            break;
    }
}

static int read_slider_raw(void) {
    int raw = 0;
    adc_oneshot_read(adc_handle, PIN_SLIDER, &raw);
    return raw; // typically around 0..4095
}

static int get_tempo_percent(void) {
    int raw = read_slider_raw();

    // Invert mapping so pushing slider "up" can feel faster if wired that way.
    // Adjust later if your slider direction feels reversed.
    int tempo = TEMPO_MAX_PERCENT - ((raw * (TEMPO_MAX_PERCENT - TEMPO_MIN_PERCENT)) / 4095);

    if (tempo < TEMPO_MIN_PERCENT) tempo = TEMPO_MIN_PERCENT;
    if (tempo > TEMPO_MAX_PERCENT) tempo = TEMPO_MAX_PERCENT;
    return tempo;
}

static int duration_ms_from_units(uint16_t units) {
    const int base_tick_ms = 100; // base rhythm unit
    int tempo_percent = get_tempo_percent();

    // duration = units * base_tick_ms * tempo_percent / 100
    int duration = (int)units * base_tick_ms * tempo_percent / 100;
    if (duration < 30) duration = 30;
    return duration;
}

static void start_song(int song_index) {
    if (song_index < 0) song_index = 0;
    if (song_index >= song_count) song_index = 0;

    player.current_song = song_index;
    player.step_index = 0;
    player.song_finished = false;
    player.led_phase = 0;
    player.step_deadline_ms = 0;

    ESP_LOGI(TAG, "Start song %d: %s", song_index + 1, song_names[song_index]);
}

static void next_song(void) {
    int next = (player.current_song + 1) % song_count;
    start_song(next);
}

static bool button_pressed_event(button_t *btn, gpio_num_t pin) {
    bool raw = gpio_get_level(pin); // pull-up: 1 idle, 0 pressed
    int64_t t = now_ms();

    if (raw != btn->last_raw) {
        btn->last_raw = raw;
        btn->last_change_ms = t;
    }

    if ((t - btn->last_change_ms) >= BUTTON_DEBOUNCE_MS) {
        if (btn->stable_state != raw) {
            btn->stable_state = raw;

            // pressed event = stable LOW
            if (btn->stable_state == 0) {
                return true;
            }
        }
    }

    return false;
}

/* ---------------- Player Engine ---------------- */
static void update_player(void) {
    const song_step_t *song = songs[player.current_song];
    int64_t t = now_ms();

    if (player.song_finished) {
        all_off();
        if (player.step_deadline_ms == 0) {
            player.step_deadline_ms = t + SONG_GAP_MS;
        } else if (t >= player.step_deadline_ms) {
            start_song(player.current_song); // loop same song
        }
        return;
    }

    if (player.step_deadline_ms != 0 && t < player.step_deadline_ms) {
        return;
    }

    song_step_t step = song[player.step_index];

    if (step.units == 0) {
        player.song_finished = true;
        player.step_deadline_ms = 0;
        all_off();
        return;
    }

    set_buzzer(step.buzzer_on);

    if (step.buzzer_on) {
        flash_led_pattern(player.led_phase++);
    } else {
        set_leds(false, false, false);
    }

    int dur_ms = duration_ms_from_units(step.units);
    player.step_deadline_ms = t + dur_ms;
    player.step_index++;
}

/* ---------------- Hardware Init ---------------- */
static void init_gpio(void) {
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << PIN_BUZZER) |
                        (1ULL << PIN_LED_RED) |
                        (1ULL << PIN_LED_YELLOW) |
                        (1ULL << PIN_LED_GREEN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&out_cfg);

    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << PIN_BTN_RED) |
                        (1ULL << PIN_BTN_BLUE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,   // button to GND when pressed
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&in_cfg);

    all_off();
}

static void init_adc(void) {
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_USED,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_USED,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, PIN_SLIDER, &chan_cfg));
}

/* ---------------- App Main ---------------- */
void app_main(void) {
    init_gpio();
    init_adc();

    ESP_LOGI(TAG, "Demo started");
    ESP_LOGI(TAG, "Red button  = restart song");
    ESP_LOGI(TAG, "Blue button = next song");
    ESP_LOGI(TAG, "Slider      = tempo/speed");

    start_song(0);

    while (1) {
        if (button_pressed_event(&btn_red, PIN_BTN_RED)) {
            ESP_LOGI(TAG, "Restart current song");
            start_song(player.current_song);
        }

        if (button_pressed_event(&btn_blue, PIN_BTN_BLUE)) {
            ESP_LOGI(TAG, "Next song");
            next_song();
        }

        update_player();

        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_MS));
    }
}