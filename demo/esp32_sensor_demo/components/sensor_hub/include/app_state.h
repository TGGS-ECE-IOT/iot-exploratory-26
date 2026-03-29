#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_event.h"
#include "esp_wifi.h"

typedef enum {
    SCREEN_HOME = 0,
    SCREEN_ENV,
    SCREEN_MOTION_LIGHT,
    SCREEN_NET,
    SCREEN_COUNT
} screen_t;

typedef struct {
    float temperature_c;
    float humidity_pct;
    float distance_cm;
    int pir_motion;
    int mq135_raw;
    int ldr_raw;
    int pot_raw;
    float mq135_pct;
    float ldr_pct;
    float pot_pct;
    uint64_t uptime_ms;
    char iso_time[32];
} sensor_data_t;

extern EventGroupHandle_t wifi_event_group;
extern SemaphoreHandle_t data_mutex;

extern int s_wifi_retry_num;
extern bool wifi_connected;
extern bool mqtt_connected;
extern esp_event_handler_instance_t wifi_any_id_instance;
extern esp_event_handler_instance_t wifi_got_ip_instance;

extern char g_config_ap_ssid[33];
extern char g_sta_target_ssid[33];
extern char g_sta_password[65];
extern char g_mqtt_host[64];
extern char g_ntp_host[64];
extern int g_mqtt_port;
extern char g_mqtt_username[33];
extern char g_mqtt_password[65];
extern char g_topic_base[33];
extern char g_site_id[33];
extern char g_node_id[33];
extern char g_node_type[17];
extern int g_pub_interval_ms;
extern char g_status_line[64];
extern char g_last_command_id[48];

extern bool g_song_playing;
extern char g_song_id[24];
extern char g_song_lyric[32];
extern int g_song_line_index;
extern int g_song_note_index;
extern int g_song_total_notes;
extern int g_song_progress_pct;

extern screen_t current_screen;
extern bool wifi_connect_in_progress;
extern bool wifi_ap_mode_enabled;

extern sensor_data_t g_sensor;

#endif
