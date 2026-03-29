#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"

#include "app_state.h"
#include "buzzer.h"
#include "pin_config.h"

typedef struct {
    int freq;
    int ms;
    const char *lyric;
    int line;
    uint8_t led_mask;
} note_t;

typedef struct {
    const note_t *song;
    int count;
    const char *song_id;
    char command_id[48];
} song_job_t;

static QueueHandle_t buzzer_queue;
static volatile bool s_stop_requested;

#define BUZZER_DUTY_MAX  1023
#define BUZZER_DUTY_TONE 512
#define BUZZER_DUTY_IDLE BUZZER_DUTY_MAX

#define LED_R (1U << 0)
#define LED_Y (1U << 1)
#define LED_G (1U << 2)

static const note_t song_ok[] = {
    {784, 120, "", 0, LED_G}, {988, 120, "", 0, LED_G}, {1319, 180, "", 0, LED_G}, {0, 80, "", 0, 0}
};

static const note_t song_warn[] = {
    {523, 150, "", 0, LED_R}, {0, 40, "", 0, 0}, {523, 150, "", 0, LED_R}, {0, 40, "", 0, 0}, {392, 220, "", 0, LED_R}
};

static const note_t song_bt[] = {
    {660, 100, "", 0, LED_Y}, {880, 100, "", 0, LED_Y}, {660, 100, "", 0, LED_Y}, {523, 180, "", 0, LED_Y}
};

static const note_t song_happy_birthday[] = {
    {264, 350, "Happy", 1, LED_R}, {264, 180, "", 1, LED_Y}, {297, 520, "Birthday", 1, LED_G},
    {264, 520, "to", 1, LED_R}, {352, 520, "you", 1, LED_Y}, {330, 900, "", 1, LED_G},
    {264, 350, "Happy", 2, LED_R}, {264, 180, "", 2, LED_Y}, {297, 520, "Birthday", 2, LED_G},
    {264, 520, "to", 2, LED_R}, {396, 520, "you", 2, LED_Y}, {352, 900, "", 2, LED_G}
};

static const note_t song_jingle_bells[] = {
    {330, 320, "Jin-gle", 1, LED_R}, {330, 320, "bells", 1, LED_Y}, {330, 650, "", 1, LED_G},
    {330, 320, "Jin-gle", 1, LED_R}, {330, 320, "bells", 1, LED_Y}, {330, 650, "", 1, LED_G},
    {330, 320, "Jin-gle", 2, LED_R}, {392, 320, "all", 2, LED_Y}, {262, 320, "the", 2, LED_G},
    {294, 320, "way", 2, LED_R}, {330, 900, "", 2, LED_Y}
};

static const note_t song_loy_krathong[] = {
    {330, 340, "Loy", 1, LED_Y}, {349, 340, "Loy", 1, LED_G}, {392, 500, "Krathong", 1, LED_R},
    {392, 340, "Loy", 1, LED_Y}, {349, 340, "Loy", 1, LED_G}, {330, 500, "Krathong", 1, LED_R},
    {294, 360, "Loy", 2, LED_Y}, {330, 360, "Loy", 2, LED_G}, {349, 700, "Krathong", 2, LED_R},
    {330, 360, "Kan", 2, LED_Y}, {294, 360, "laew", 2, LED_G}, {262, 800, "", 2, LED_R}
};

static void set_led_mask(uint8_t mask) {
    gpio_set_level(PIN_LED_RED, (mask & LED_R) ? 1 : 0);
    gpio_set_level(PIN_LED_YELLOW, (mask & LED_Y) ? 1 : 0);
    gpio_set_level(PIN_LED_GREEN, (mask & LED_G) ? 1 : 0);
}

static void set_playback_state(bool active, const char *song_id, const char *lyric,
                               int line, int note_idx, int total_count) {
    if (data_mutex != NULL) {
        xSemaphoreTake(data_mutex, portMAX_DELAY);
    }

    g_song_playing = active;
    if (song_id != NULL) {
        strncpy(g_song_id, song_id, sizeof(g_song_id) - 1);
        g_song_id[sizeof(g_song_id) - 1] = '\0';
    }
    if (lyric != NULL) {
        strncpy(g_song_lyric, lyric, sizeof(g_song_lyric) - 1);
        g_song_lyric[sizeof(g_song_lyric) - 1] = '\0';
    }
    g_song_line_index = line;
    g_song_note_index = note_idx;
    g_song_total_notes = total_count;
    g_song_progress_pct = (total_count > 0) ? (note_idx * 100) / total_count : 0;

    if (data_mutex != NULL) {
        xSemaphoreGive(data_mutex);
    }
}

static bool buzzer_tone_interruptible(int freq_hz, int duration_ms, uint8_t led_mask) {
    if (freq_hz > 0) {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq_hz);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, BUZZER_DUTY_TONE);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        set_led_mask(led_mask);
    } else {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, BUZZER_DUTY_IDLE);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        set_led_mask(0);
    }

    int remain = duration_ms;
    while (remain > 0) {
        if (s_stop_requested) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, BUZZER_DUTY_IDLE);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            set_led_mask(0);
            return false;
        }
        int step = remain > 20 ? 20 : remain;
        vTaskDelay(pdMS_TO_TICKS(step));
        remain -= step;
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, BUZZER_DUTY_IDLE);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    set_led_mask(0);
    return true;
}

static void play_song_async(const note_t *song, int count, const char *song_id, const char *command_id) {
    song_job_t job;
    memset(&job, 0, sizeof(job));
    job.song = song;
    job.count = count;
    job.song_id = song_id;
    if (command_id != NULL) {
        strncpy(job.command_id, command_id, sizeof(job.command_id) - 1);
        job.command_id[sizeof(job.command_id) - 1] = '\0';
    }
    if (buzzer_queue != NULL) {
        s_stop_requested = false;
        xQueueSend(buzzer_queue, &job, 0);
    }
}

void buzzer_init(void) {
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_channel_config_t ch_cfg = {
        .gpio_num = PIN_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = BUZZER_DUTY_IDLE,
        .hpoint = 0,
    };

    ledc_timer_config(&timer_cfg);
    ledc_channel_config(&ch_cfg);

    buzzer_queue = xQueueCreate(8, sizeof(song_job_t));
}

void buzzer_task(void *arg) {
    (void)arg;
    song_job_t job;
    while (1) {
        if (xQueueReceive(buzzer_queue, &job, portMAX_DELAY) == pdTRUE) {
            if (job.command_id[0] != '\0') {
                strncpy(g_last_command_id, job.command_id, sizeof(g_last_command_id) - 1);
                g_last_command_id[sizeof(g_last_command_id) - 1] = '\0';
            }

            set_playback_state(true, job.song_id, "", 0, 0, job.count);
            for (int i = 0; i < job.count; i++) {
                const char *lyric = (job.song[i].lyric != NULL) ? job.song[i].lyric : "";
                set_playback_state(true, job.song_id, lyric, job.song[i].line, i + 1, job.count);
                if (!buzzer_tone_interruptible(job.song[i].freq, job.song[i].ms, job.song[i].led_mask)) {
                    break;
                }
            }

            set_playback_state(false, "", "", 0, 0, 0);
            s_stop_requested = false;
            set_led_mask(0);
        }
    }
}

void buzzer_play_ok(void) {
    play_song_async(song_ok, sizeof(song_ok) / sizeof(song_ok[0]), "ok", "");
}

void buzzer_play_ok_short(void) {
    play_song_async(song_ok, 2, "ok", "");
}

void buzzer_play_warn(void) {
    play_song_async(song_warn, sizeof(song_warn) / sizeof(song_warn[0]), "warn", "");
}

void buzzer_play_warn_short(void) {
    play_song_async(song_warn, 2, "warn", "");
}

void buzzer_play_bt(void) {
    play_song_async(song_bt, sizeof(song_bt) / sizeof(song_bt[0]), "bt", "");
}

void buzzer_play_song(buzzer_song_id_t song_id, const char *command_id) {
    switch (song_id) {
        case SONG_HAPPY_BIRTHDAY:
            play_song_async(song_happy_birthday,
                            sizeof(song_happy_birthday) / sizeof(song_happy_birthday[0]),
                            "happy_birthday", command_id);
            break;
        case SONG_JINGLE_BELLS:
            play_song_async(song_jingle_bells,
                            sizeof(song_jingle_bells) / sizeof(song_jingle_bells[0]),
                            "jingle_bells", command_id);
            break;
        case SONG_LOY_KRATHONG:
            play_song_async(song_loy_krathong,
                            sizeof(song_loy_krathong) / sizeof(song_loy_krathong[0]),
                            "loy_krathong", command_id);
            break;
        default:
            break;
    }
}

void buzzer_stop(void) {
    s_stop_requested = true;
    if (buzzer_queue != NULL) {
        xQueueReset(buzzer_queue);
    }
}
