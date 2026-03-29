#include <string.h>

#include "app_state.h"

EventGroupHandle_t wifi_event_group;
SemaphoreHandle_t data_mutex;

int s_wifi_retry_num = 0;
bool wifi_connected = false;
bool mqtt_connected = false;
esp_event_handler_instance_t wifi_any_id_instance;
esp_event_handler_instance_t wifi_got_ip_instance;

char g_config_ap_ssid[33] = "TGGS_SENSOR_NODE";
char g_sta_target_ssid[33] = "<not set>";
char g_sta_password[65] = "";
char g_mqtt_host[64] = "192.168.50.1";
char g_ntp_host[64] = "192.168.50.1";
int g_mqtt_port = 1883;
char g_mqtt_username[33] = "";
char g_mqtt_password[65] = "";
char g_topic_base[33] = "tggs/v1";
char g_site_id[33] = "demo-site";
char g_node_id[33] = "sensor-001";
char g_node_type[17] = "sensor";
int g_pub_interval_ms = 2000;
char g_status_line[64] = "Booting...";
char g_last_command_id[48] = "";

bool g_song_playing = false;
char g_song_id[24] = "";
char g_song_lyric[32] = "";
int g_song_line_index = 0;
int g_song_note_index = 0;
int g_song_total_notes = 0;
int g_song_progress_pct = 0;

screen_t current_screen = SCREEN_HOME;
bool wifi_connect_in_progress = false;
bool wifi_ap_mode_enabled = false;

sensor_data_t g_sensor = {0};
