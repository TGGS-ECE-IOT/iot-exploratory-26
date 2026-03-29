#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mqtt_client.h"

#include "app_state.h"
#include "buzzer.h"
#include "mqtt_manager.h"
#include "status.h"
#include "utils.h"

#define TAG "IOT_MQTT"

static esp_mqtt_client_handle_t s_mqtt_client;
static uint32_t s_msg_seq;

static void build_up_topic(char *out, size_t out_len, const char *leaf) {
    snprintf(out, out_len, "%.32s/%.32s/%.32s/up/%s", g_topic_base, g_site_id, g_node_id, leaf);
}

static void build_down_topic(char *out, size_t out_len, const char *leaf) {
    snprintf(out, out_len, "%.32s/%.32s/%.32s/down/%s", g_topic_base, g_site_id, g_node_id, leaf);
}

static void make_msg_id(char *out, size_t out_len, const char *prefix) {
    unsigned long long t = (unsigned long long)now_ms();
    unsigned int seq = ++s_msg_seq;
    snprintf(out, out_len, "%s-%llu-%u", prefix, t, seq);
}

static bool json_get_string(const char *json, int len, const char *key, char *out, size_t out_len) {
    char pattern[40];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char *start = strstr(json, pattern);
    if (start == NULL) return false;

    start += strlen(pattern);
    const char *end = start;
    const char *limit = json + len;
    while (end < limit && *end != '"') end++;
    if (end >= limit) return false;

    size_t n = (size_t)(end - start);
    if (n >= out_len) n = out_len - 1;
    memcpy(out, start, n);
    out[n] = '\0';
    return true;
}

static bool topic_equals(const char *topic, int topic_len, const char *expected) {
    size_t n = strlen(expected);
    return ((int)n == topic_len) && (memcmp(topic, expected, n) == 0);
}

static void publish_ack(const char *command_id, const char *ack_status, const char *detail) {
    if (!mqtt_connected || s_mqtt_client == NULL) return;

    char topic[192];
    char msg_id[40];
    char ts[32];
    char json[512];

    build_up_topic(topic, sizeof(topic), "ack");
    make_msg_id(msg_id, sizeof(msg_id), "ack");
    get_iso_time(ts, sizeof(ts));

    snprintf(json, sizeof(json),
             "{\"schema\":\"tggs.node.packet.v1\",\"msg_id\":\"%.39s\",\"ts\":\"%.31s\","
             "\"site_id\":\"%.32s\",\"node_id\":\"%.32s\",\"type\":\"ack.command\","
             "\"payload\":{\"command_id\":\"%.47s\",\"status\":\"%.15s\",\"detail\":\"%.47s\"}}",
             msg_id, ts, g_site_id, g_node_id, command_id, ack_status, detail);

    esp_mqtt_client_publish(s_mqtt_client, topic, json, 0, 1, 0);
}

static void publish_playback_status(const char *state,
                                    const char *command_id,
                                    const char *song_id,
                                    int line_index,
                                    int note_index,
                                    int progress_pct) {
    if (!mqtt_connected || s_mqtt_client == NULL) return;

    char topic[192];
    char msg_id[40];
    char ts[32];
    char json[512];

    build_up_topic(topic, sizeof(topic), "status/playback");
    make_msg_id(msg_id, sizeof(msg_id), "play");
    get_iso_time(ts, sizeof(ts));

    snprintf(json, sizeof(json),
             "{\"schema\":\"tggs.node.packet.v1\",\"msg_id\":\"%.39s\",\"ts\":\"%.31s\","
             "\"site_id\":\"%.32s\",\"node_id\":\"%.32s\",\"type\":\"status.playback\","
             "\"payload\":{\"command_id\":\"%.47s\",\"state\":\"%.15s\",\"song_id\":\"%.23s\","
             "\"line_index\":%d,\"note_index\":%d,\"progress_pct\":%d}}",
             msg_id, ts, g_site_id, g_node_id, command_id, state, song_id,
             line_index, note_index, progress_pct);

    esp_mqtt_client_publish(s_mqtt_client, topic, json, 0, 1, 0);
}

static void publish_periodic(const sensor_data_t *s) {
    if (!mqtt_connected || s_mqtt_client == NULL || s == NULL) return;

    char topic[192];
    char msg_id[40];
    char ts[32];
    char json[768];

    build_up_topic(topic, sizeof(topic), "telemetry/periodic");
    make_msg_id(msg_id, sizeof(msg_id), "tele");
    get_iso_time(ts, sizeof(ts));

    snprintf(json, sizeof(json),
             "{\"schema\":\"tggs.node.packet.v1\",\"msg_id\":\"%.39s\",\"ts\":\"%.31s\","
             "\"site_id\":\"%.32s\",\"node_id\":\"%.32s\",\"type\":\"telemetry.periodic\","
             "\"payload\":{\"period_sec\":%u,\"sensors\":{"
             "\"dht22\":{\"temp_c\":%.2f,\"humidity_pct\":%.2f},"
             "\"air_quality\":{\"raw\":%d,\"aq_index\":%.1f},"
             "\"ultrasonic\":{\"distance_cm\":%.2f},"
             "\"ldr\":{\"raw\":%d,\"lux_est\":%.1f}},"
             "\"device\":{\"uptime_ms\":%llu}}}",
             msg_id, ts, g_site_id, g_node_id,
             (unsigned)(g_pub_interval_ms / 1000),
             s->temperature_c, s->humidity_pct,
             s->mq135_raw, s->mq135_pct,
             s->distance_cm,
             s->ldr_raw, s->ldr_pct,
             (unsigned long long)s->uptime_ms);

    esp_mqtt_client_publish(s_mqtt_client, topic, json, 0, 0, 0);
}

static void publish_pir_event(int value, const char *edge) {
    if (!mqtt_connected || s_mqtt_client == NULL) return;

    char topic[192];
    char msg_id[40];
    char ts[32];
    char json[512];

    build_up_topic(topic, sizeof(topic), "event/pir");
    make_msg_id(msg_id, sizeof(msg_id), "pir");
    get_iso_time(ts, sizeof(ts));

    snprintf(json, sizeof(json),
             "{\"schema\":\"tggs.node.packet.v1\",\"msg_id\":\"%.39s\",\"ts\":\"%.31s\","
             "\"site_id\":\"%.32s\",\"node_id\":\"%.32s\",\"type\":\"event.pir\","
             "\"payload\":{\"event\":\"%s\",\"value\":%d,\"edge\":\"%.7s\",\"cooldown_ms\":2000}}",
             msg_id, ts, g_site_id, g_node_id,
             value ? "motion_detected" : "motion_clear", value, edge);

    esp_mqtt_client_publish(s_mqtt_client, topic, json, 0, 1, 0);
}

static void publish_pot_event(float prev_pct, float curr_pct, int raw) {
    if (!mqtt_connected || s_mqtt_client == NULL) return;

    char topic[192];
    char msg_id[40];
    char ts[32];
    char json[512];

    build_up_topic(topic, sizeof(topic), "event/pot");
    make_msg_id(msg_id, sizeof(msg_id), "pot");
    get_iso_time(ts, sizeof(ts));

    snprintf(json, sizeof(json),
             "{\"schema\":\"tggs.node.packet.v1\",\"msg_id\":\"%.39s\",\"ts\":\"%.31s\","
             "\"site_id\":\"%.32s\",\"node_id\":\"%.32s\",\"type\":\"event.pot\","
             "\"payload\":{\"value_raw\":%d,\"value_pct\":%.2f,\"debounce_ms\":500,"
             "\"change\":{\"prev_pct\":%.2f,\"delta_pct\":%.2f}}}",
             msg_id, ts, g_site_id, g_node_id,
             raw, curr_pct, prev_pct, fabsf(curr_pct - prev_pct));

    esp_mqtt_client_publish(s_mqtt_client, topic, json, 0, 1, 0);
}

static bool parse_song_id(const char *song) {
    return (strcmp(song, "happy_birthday") == 0) ||
           (strcmp(song, "jingle_bells") == 0) ||
           (strcmp(song, "loy_krathong") == 0);
}

static buzzer_song_id_t song_to_id(const char *song) {
    if (strcmp(song, "happy_birthday") == 0) return SONG_HAPPY_BIRTHDAY;
    if (strcmp(song, "jingle_bells") == 0) return SONG_JINGLE_BELLS;
    if (strcmp(song, "loy_krathong") == 0) return SONG_LOY_KRATHONG;
    return SONG_NONE;
}

static void handle_playback_command(const char *data, int len) {
    char action[16] = {0};
    char song_id[24] = {0};
    char command_id[48] = {0};

    if (!json_get_string(data, len, "action", action, sizeof(action))) {
        publish_ack("", "rejected", "missing action");
        return;
    }
    if (!json_get_string(data, len, "song_id", song_id, sizeof(song_id))) {
        song_id[0] = '\0';
    }
    if (!json_get_string(data, len, "command_id", command_id, sizeof(command_id))) {
        make_msg_id(command_id, sizeof(command_id), "cmd");
    }

    if (strcmp(action, "play") == 0) {
        if (!parse_song_id(song_id)) {
            publish_ack(command_id, "rejected", "invalid song_id");
            return;
        }
        buzzer_play_song(song_to_id(song_id), command_id);
        set_status("Cmd play: %.16s", song_id);
        publish_ack(command_id, "accepted", "queued");
    } else if (strcmp(action, "stop") == 0) {
        buzzer_stop();
        set_status("Cmd stop playback");
        publish_ack(command_id, "accepted", "stopped");
    } else if (strcmp(action, "pause") == 0 || strcmp(action, "resume") == 0) {
        publish_ack(command_id, "rejected", "unsupported action");
    } else {
        publish_ack(command_id, "rejected", "unknown action");
    }
}

static void handle_command_topic(const char *topic, int topic_len, const char *data, int len) {
    char playback_topic[192];
    char display_topic[192];
    char led_topic[192];

    build_down_topic(playback_topic, sizeof(playback_topic), "cmd/playback");
    build_down_topic(display_topic, sizeof(display_topic), "cmd/display");
    build_down_topic(led_topic, sizeof(led_topic), "cmd/led");

    if (topic_equals(topic, topic_len, playback_topic)) {
        handle_playback_command(data, len);
        return;
    }

    if (topic_equals(topic, topic_len, display_topic) || topic_equals(topic, topic_len, led_topic)) {
        char command_id[48] = {0};
        if (!json_get_string(data, len, "command_id", command_id, sizeof(command_id))) {
            make_msg_id(command_id, sizeof(command_id), "cmd");
        }
        publish_ack(command_id, "rejected", "not implemented");
        return;
    }

    if (strstr(data, "\"op\":\"reboot\"")) {
        set_status("Cmd: reboot");
        esp_restart();
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED: {
            char playback_topic[192];
            char display_topic[192];
            char led_topic[192];

            mqtt_connected = true;
            set_status("MQTT connected");
            ESP_LOGI(TAG, "MQTT connected");

            build_down_topic(playback_topic, sizeof(playback_topic), "cmd/playback");
            build_down_topic(display_topic, sizeof(display_topic), "cmd/display");
            build_down_topic(led_topic, sizeof(led_topic), "cmd/led");

            esp_mqtt_client_subscribe(s_mqtt_client, playback_topic, 1);
            esp_mqtt_client_subscribe(s_mqtt_client, display_topic, 1);
            esp_mqtt_client_subscribe(s_mqtt_client, led_topic, 1);
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            set_status("MQTT disconnected");
            ESP_LOGW(TAG, "MQTT disconnected");
            break;
        case MQTT_EVENT_DATA:
            {
                char topic[192];
                char data[512];
                int tlen = event->topic_len;
                int dlen = event->data_len;

                if (tlen >= (int)sizeof(topic)) tlen = (int)sizeof(topic) - 1;
                if (dlen >= (int)sizeof(data)) dlen = (int)sizeof(data) - 1;

                memcpy(topic, event->topic, tlen);
                topic[tlen] = '\0';
                memcpy(data, event->data, dlen);
                data[dlen] = '\0';

                handle_command_topic(topic, tlen, data, dlen);
            }
            break;
        default:
            break;
    }
}

void mqtt_manager_start(void) {
    if (s_mqtt_client != NULL) {
        return;
    }

    char uri[192];
    snprintf(uri, sizeof(uri), "mqtt://%.63s:%d", g_mqtt_host, g_mqtt_port);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = uri,
        .credentials.username = g_mqtt_username[0] ? g_mqtt_username : NULL,
        .credentials.authentication.password = g_mqtt_password[0] ? g_mqtt_password : NULL,
    };

    s_mqtt_client = esp_mqtt_client_init(&cfg);
    if (s_mqtt_client == NULL) {
        set_status("MQTT init fail");
        return;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
}

void mqtt_manager_stop(void) {
    if (s_mqtt_client == NULL) {
        return;
    }

    esp_mqtt_client_stop(s_mqtt_client);
    esp_mqtt_client_destroy(s_mqtt_client);
    s_mqtt_client = NULL;
    mqtt_connected = false;
}

void mqtt_tx_task(void *arg) {
    (void)arg;

    uint64_t next_periodic_at = 0;
    int last_pir = -1;
    bool last_song_playing = false;
    int last_song_note = -1;
    char last_song_id[24] = {0};
    char last_command_id[48] = {0};
    float last_pot_sent = -1000.0f;
    float pot_stable_value = 0.0f;
    uint64_t pot_changed_at = 0;
    bool pot_initialized = false;

    while (1) {
        sensor_data_t s;
        bool song_playing;
        char song_id[24];
        char command_id[48];
        int song_line;
        int song_note;
        int song_progress;
        uint64_t now;

        xSemaphoreTake(data_mutex, portMAX_DELAY);
        s = g_sensor;
        song_playing = g_song_playing;
        snprintf(song_id, sizeof(song_id), "%.23s", g_song_id);
        snprintf(command_id, sizeof(command_id), "%.47s", g_last_command_id);
        song_line = g_song_line_index;
        song_note = g_song_note_index;
        song_progress = g_song_progress_pct;
        xSemaphoreGive(data_mutex);

        now = now_ms();

        if (mqtt_connected && s_mqtt_client != NULL) {
            if (next_periodic_at == 0 || now >= next_periodic_at) {
                publish_periodic(&s);
                next_periodic_at = now + (uint64_t)(g_pub_interval_ms > 500 ? g_pub_interval_ms : 2000);
            }

            if (last_pir < 0) {
                last_pir = s.pir_motion;
            } else if (s.pir_motion != last_pir) {
                publish_pir_event(s.pir_motion, s.pir_motion ? "rising" : "falling");
                last_pir = s.pir_motion;
            }

            if (!pot_initialized) {
                pot_stable_value = s.pot_pct;
                pot_changed_at = now;
                last_pot_sent = s.pot_pct;
                pot_initialized = true;
            } else {
                if (fabsf(s.pot_pct - pot_stable_value) > 0.8f) {
                    pot_stable_value = s.pot_pct;
                    pot_changed_at = now;
                } else if ((now - pot_changed_at) >= 500 && fabsf(s.pot_pct - last_pot_sent) >= 2.0f) {
                    publish_pot_event(last_pot_sent, s.pot_pct, s.pot_raw);
                    last_pot_sent = s.pot_pct;
                }
            }

            if (song_playing && !last_song_playing) {
                snprintf(last_song_id, sizeof(last_song_id), "%.23s", song_id);
                snprintf(last_command_id, sizeof(last_command_id), "%.47s", command_id);
                publish_playback_status("playing", command_id, song_id, song_line, song_note, song_progress);
            } else if (!song_playing && last_song_playing) {
                publish_playback_status("completed", last_command_id, last_song_id, song_line, song_note, 100);
            } else if (song_playing && song_note != last_song_note) {
                publish_playback_status("playing", command_id, song_id, song_line, song_note, song_progress);
            }
        }

        last_song_playing = song_playing;
        last_song_note = song_note;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
