#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_state.h"
#include "oled_driver.h"
#include "splash_assets.h"
#include "ui.h"

void ui_init(void) {
    oled_driver_init();
}

static void draw_splash_top_banner(void) {
    oled_set_font_small();
    oled_draw_xbm(2, 2, WIFI_ICON_WIDTH, WIFI_ICON_HEIGHT, wifi_icon_12x12_bits);
    oled_draw_xbm(114, 2, MQTT_ICON_WIDTH, MQTT_ICON_HEIGHT, mqtt_icon_12x12_bits);
    oled_draw_text_centered(12, "IOT Exploratory 2026");
    oled_draw_line(0, 16, 127, 16);
}

void ui_show_boot_splash(void) {
    oled_clear_buffer();
    draw_splash_top_banner();
    oled_draw_xbm(0, 16, ECE_LOGO_WIDTH, ECE_LOGO_HEIGHT, ece_logo_128x48_bits);
    oled_refresh();
    vTaskDelay(pdMS_TO_TICKS(1000));

    oled_clear_buffer();
    draw_splash_top_banner();
    oled_set_font_large();
    oled_draw_text_centered(38, "TGGS");
    oled_draw_text_centered(60, "KMUTNB");
    oled_refresh();
    vTaskDelay(pdMS_TO_TICKS(1000));

    oled_set_font_small();
}

static void ui_render(void) {
    sensor_data_t s;
    bool song_playing;
    char song_id[24];
    char song_lyric[32];
    int song_line;
    int song_progress;
    char dt_date[12];
    char dt_time[10];
    char status0[16];
    char status1[16];
    char line2[32];
    char line3[32];
    char line4[32];
    char line5[32];
    char line6[32];
    char line7[32];

    xSemaphoreTake(data_mutex, portMAX_DELAY);
    s = g_sensor;
    song_playing = g_song_playing;
    snprintf(song_id, sizeof(song_id), "%.23s", g_song_id);
    snprintf(song_lyric, sizeof(song_lyric), "%.31s", g_song_lyric);
    song_line = g_song_line_index;
    song_progress = g_song_progress_pct;
    xSemaphoreGive(data_mutex);

    oled_clear_buffer();

    /* Top yellow band (rows 0-15): date/time left, status right */
    if (strlen(s.iso_time) >= 19 && s.iso_time[4] == '-' && s.iso_time[7] == '-') {
        snprintf(dt_date, sizeof(dt_date), "%.10s", s.iso_time);
        snprintf(dt_time, sizeof(dt_time), "%.8s", &s.iso_time[11]);
    } else {
        snprintf(dt_date, sizeof(dt_date), "UPTIME");
        snprintf(dt_time, sizeof(dt_time), "%08lu", (unsigned long)((s.uptime_ms / 1000ULL) % 100000000ULL));
    }

    snprintf(status0, sizeof(status0), "W:%s M:%s",
             wifi_connected ? "OK" : "NO",
             mqtt_connected ? "OK" : "NO");
    snprintf(status1, sizeof(status1), "%.12s", g_status_line);

    oled_draw_text(0, 0, dt_date);
    oled_draw_text(0, 1, dt_time);
    oled_draw_text_right(7, status0);
    oled_draw_text_right(15, status1);

    /* Blue band (rows 16-63): sensor view if RPi connected, otherwise setup view */
    if (song_playing) {
        snprintf(line2, sizeof(line2), "Cloud song active");
        snprintf(line3, sizeof(line3), "Song: %.18s", song_id[0] ? song_id : "unknown");
        snprintf(line4, sizeof(line4), "Line:%d P:%d%%", song_line, song_progress);
        snprintf(line5, sizeof(line5), "Lyric:");
        snprintf(line6, sizeof(line6), "%.20s", song_lyric[0] ? song_lyric : "...");
        snprintf(line7, sizeof(line7), "RGB blink in rhythm");
    } else if (wifi_connected && mqtt_connected) {
        snprintf(line2, sizeof(line2), "NODE: %.14s", g_node_id);
        snprintf(line3, sizeof(line3), "T:%.1fC H:%.1f%%", s.temperature_c, s.humidity_pct);
        snprintf(line4, sizeof(line4), "D:%.1fcm PIR:%d", s.distance_cm, s.pir_motion);
        snprintf(line5, sizeof(line5), "MQ:%d LDR:%d", s.mq135_raw, s.ldr_raw);
        snprintf(line6, sizeof(line6), "POT:%d UP:%08lu", s.pot_raw,
                 (unsigned long)((s.uptime_ms / 1000ULL) % 100000000ULL));
        snprintf(line7, sizeof(line7), "RPi OK MQTT:%.16s", g_mqtt_host);
    } else {
        snprintf(line2, sizeof(line2), "RPi not connected");
        snprintf(line3, sizeof(line3), "AP: %.18s", g_config_ap_ssid);
        snprintf(line4, sizeof(line4), "PASS: tggs_kmutnb");
        snprintf(line5, sizeof(line5), "URL: sensor.tggs");
        snprintf(line6, sizeof(line6), "IP: 192.168.4.1");
        snprintf(line7, sizeof(line7), "MQTT: %.18s", g_mqtt_host);
    }

    oled_draw_text(0, 2, line2);
    oled_draw_text(0, 3, line3);
    oled_draw_text(0, 4, line4);
    oled_draw_text(0, 5, line5);
    oled_draw_text(0, 6, line6);
    oled_draw_text(0, 7, line7);

    oled_refresh();
}

void ui_task(void *arg) {
    while (1) {
        ui_render();
        vTaskDelay(pdMS_TO_TICKS(120));
    }
}
