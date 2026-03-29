#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "mqtt_client.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "button.h"
#include "buzzer.h"
#include "ece_logo_128x32.h"
#include "led.h"
#include "oled.h"
#include "post.h"
#include "relay.h"
#include "servo.h"

#define AP_SSID "IOT_ACTUATOR_DEMO"
#define AP_PASS "tggs_kmutnb"
#define AP_DNS_NAME "actuator.tggs"
#define AP_IP_ADDR "192.168.4.1"

#define MQTT_PROTO_VERSION 1

#define WIFI_CONNECTED_BIT BIT0
#define MQTT_CONNECTED_BIT BIT1

#define WIFI_CONNECT_TIMEOUT_MS 20000
#define MQTT_CONNECT_TIMEOUT_MS 15000

#define OLED_CMD_SHOW_MS 1500
#define OLED_CUSTOM_SHOW_MS 5000

#define DEVICE_ID_LEN 32
#define TOPIC_LEN 128
#define MSG_ID_LEN 64
#define LAST_CMD_TEXT_LEN 48

#define LED_MODE_OFF (-1)
#define DNS_SERVER_PORT 53

typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
    char mqtt_uri[128];
    char mqtt_user[65];
    char mqtt_pass[65];
    char mqtt_topic[65];
    char ntp_server[65];
    uint8_t valid;
} app_config_t;

typedef struct {
    bool wifi_ok;
    bool mqtt_ok;
    bool time_ok;
    bool ap_mode;
    bool relay_on;
    int servo_angle;
    int led_mode;
    bool buzzer_active;
} runtime_status_t;

static const char *TAG = "MAIN";

static EventGroupHandle_t s_evt_group;
static esp_mqtt_client_handle_t s_mqtt_client;
static httpd_handle_t s_httpd;
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static TaskHandle_t s_dns_task_handle;
static int s_dns_sock = -1;
static volatile bool s_dns_running;

static app_config_t s_cfg;
static runtime_status_t s_status;

static volatile bool s_reboot_requested;

static char s_device_id[DEVICE_ID_LEN];
static char s_topic_cmd[TOPIC_LEN];
static char s_topic_ack[TOPIC_LEN];
static char s_topic_status[TOPIC_LEN];
static char s_topic_availability[TOPIC_LEN];

static char s_last_cmd_id[MSG_ID_LEN];
static char s_last_error[64];
static char s_last_cmd_text[LAST_CMD_TEXT_LEN];

static TickType_t s_oled_custom_until;
static char s_oled_custom_line1[22];
static char s_oled_custom_line2[22];
static char s_oled_custom_line3[22];
static char s_oled_custom_line4[22];
static bool s_ap_screen_drawn;
static runtime_status_t s_oled_prev_status;
static char s_oled_prev_cmd_text[LAST_CMD_TEXT_LEN];
static bool s_oled_prev_valid;

static oled_config_t s_oled;
static led_config_t s_led_red;
static led_config_t s_led_yellow;
static led_config_t s_led_green;
static relay_config_t s_relay;
static servo_config_t s_servo;
static buzzer_config_t s_buzzer;
static button_config_t s_button;

static void strlcpy_safe(char *dst, size_t dst_len, const char *src)
{
    if (dst_len == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

static void format_timestamp_iso(char *out, size_t out_len)
{
    time_t now = time(NULL);
    struct tm tm_utc;

    if (now <= 0 || !gmtime_r(&now, &tm_utc)) {
        strlcpy_safe(out, out_len, "1970-01-01T00:00:00Z");
        return;
    }
    strftime(out, out_len, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

static void build_device_id(void)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_device_id, sizeof(s_device_id), "esp32-%02X%02X%02X", mac[3], mac[4], mac[5]);
}

static void topic_resolve_device_id(const char *templ, char *out, size_t out_len)
{
    const char *tag = "{deviceId}";
    const char *pos;

    if (!templ || templ[0] == '\0') {
        snprintf(out, out_len, "iot/actuator/%s/cmd", s_device_id);
        return;
    }

    pos = strstr(templ, tag);
    if (!pos) {
        strlcpy_safe(out, out_len, templ);
        return;
    }

    {
        size_t prefix_len = (size_t)(pos - templ);
        size_t suffix_len = strlen(pos + strlen(tag));
        if (prefix_len + strlen(s_device_id) + suffix_len + 1 > out_len) {
            strlcpy_safe(out, out_len, templ);
            return;
        }

        memcpy(out, templ, prefix_len);
        out[prefix_len] = '\0';
        strcat(out, s_device_id);
        strcat(out, pos + strlen(tag));
    }
}

static bool topic_join(char *dst, size_t dst_len, const char *base, const char *suffix)
{
    size_t base_len = strlen(base);
    size_t suffix_len = strlen(suffix);

    if (base_len + suffix_len + 1 > dst_len) {
        return false;
    }

    memcpy(dst, base, base_len);
    memcpy(dst + base_len, suffix, suffix_len + 1);
    return true;
}

static void build_topics(void)
{
    char base[TOPIC_LEN];
    size_t cmd_len;

    topic_resolve_device_id(s_cfg.mqtt_topic, s_topic_cmd, sizeof(s_topic_cmd));

    cmd_len = strlen(s_topic_cmd);
    if (cmd_len > 4 && strcmp(s_topic_cmd + cmd_len - 4, "/cmd") == 0) {
        memcpy(base, s_topic_cmd, cmd_len - 4);
        base[cmd_len - 4] = '\0';
    } else {
        snprintf(base, sizeof(base), "iot/actuator/%s", s_device_id);
    }

    if (!topic_join(s_topic_ack, sizeof(s_topic_ack), base, "/ack") ||
        !topic_join(s_topic_status, sizeof(s_topic_status), base, "/status") ||
        !topic_join(s_topic_availability, sizeof(s_topic_availability), base, "/availability")) {
        snprintf(base, sizeof(base), "iot/actuator/%s", s_device_id);
        topic_join(s_topic_ack, sizeof(s_topic_ack), base, "/ack");
        topic_join(s_topic_status, sizeof(s_topic_status), base, "/status");
        topic_join(s_topic_availability, sizeof(s_topic_availability), base, "/availability");
    }
}

static void set_led_mode(int mode)
{
    if (mode == LED_RED) {
        led_traffic_set(&s_led_red, &s_led_yellow, &s_led_green, LED_RED);
    } else if (mode == LED_YELLOW) {
        led_traffic_set(&s_led_red, &s_led_yellow, &s_led_green, LED_YELLOW);
    } else if (mode == LED_GREEN) {
        led_traffic_set(&s_led_red, &s_led_yellow, &s_led_green, LED_GREEN);
    } else {
        led_off(&s_led_red);
        led_off(&s_led_yellow);
        led_off(&s_led_green);
    }
    s_status.led_mode = mode;
}

static void apply_actuator_state(void)
{
    if (s_status.relay_on) {
        relay_on(&s_relay);
    } else {
        relay_off(&s_relay);
    }

    servo_set_angle(&s_servo, (uint32_t)s_status.servo_angle);
    set_led_mode(s_status.led_mode);
}

static void oled_show_status_screen(void)
{
    char line[24];

    oled_clear(&s_oled);

    snprintf(line, sizeof(line), "WIFI:%s", s_status.wifi_ok ? "OK" : (s_status.ap_mode ? "AP" : "NO"));
    oled_draw_str(&s_oled, 0, 0, line);

    snprintf(line, sizeof(line), "MQTT:%s", s_status.mqtt_ok ? "OK" : "NO");
    oled_draw_str(&s_oled, 0, 8, line);

    snprintf(line, sizeof(line), "TIME:%s", s_status.time_ok ? "OK" : "NO");
    oled_draw_str(&s_oled, 0, 16, line);

    oled_draw_str(&s_oled, 0, 24, s_status.ap_mode ? "MODE:CFG" : "MODE:RUN");

    oled_draw_str(&s_oled, 66, 0, s_status.relay_on ? "FAN:ON" : "FAN:OFF");
    snprintf(line, sizeof(line), "SRV:%03d", s_status.servo_angle);
    oled_draw_str(&s_oled, 66, 8, line);

    if (s_status.led_mode == LED_RED) {
        oled_draw_str(&s_oled, 66, 16, "LED:RED");
    } else if (s_status.led_mode == LED_YELLOW) {
        oled_draw_str(&s_oled, 66, 16, "LED:YEL");
    } else if (s_status.led_mode == LED_GREEN) {
        oled_draw_str(&s_oled, 66, 16, "LED:GRN");
    } else {
        oled_draw_str(&s_oled, 66, 16, "LED:OFF");
    }

    oled_draw_str(&s_oled, 66, 24, "SRC:MQTT");
    oled_update_display(&s_oled);
}

static void oled_show_custom(const char *l1, const char *l2, const char *l3, const char *l4, TickType_t duration_ticks)
{
    strlcpy_safe(s_oled_custom_line1, sizeof(s_oled_custom_line1), l1 ? l1 : "");
    strlcpy_safe(s_oled_custom_line2, sizeof(s_oled_custom_line2), l2 ? l2 : "");
    strlcpy_safe(s_oled_custom_line3, sizeof(s_oled_custom_line3), l3 ? l3 : "");
    strlcpy_safe(s_oled_custom_line4, sizeof(s_oled_custom_line4), l4 ? l4 : "");
    s_oled_custom_until = xTaskGetTickCount() + duration_ticks;
}

static void oled_show_cmd_received(const char *cmd_text)
{
    char line[22];

    snprintf(line, sizeof(line), "CMD:%s", cmd_text);
    oled_show_custom("MQTT RX", line, "", "", pdMS_TO_TICKS(OLED_CMD_SHOW_MS));
}

static void oled_render(void)
{
    TickType_t now = xTaskGetTickCount();

    if (now < s_oled_custom_until) {
        s_ap_screen_drawn = false;
        s_oled_prev_valid = false;
        oled_clear(&s_oled);
        oled_draw_str(&s_oled, 0, 0, s_oled_custom_line1);
        oled_draw_str(&s_oled, 0, 8, s_oled_custom_line2);
        oled_draw_str(&s_oled, 0, 16, s_oled_custom_line3);
        oled_draw_str(&s_oled, 0, 24, s_oled_custom_line4);
        oled_update_display(&s_oled);
        return;
    }

    if (s_status.ap_mode) {
        if (s_ap_screen_drawn) {
            return;
        }
        s_oled_prev_valid = false;
        oled_clear(&s_oled);
        oled_draw_str(&s_oled, 0, 0, "AP:IOT_ACTUATOR");
        oled_draw_str(&s_oled, 0, 8, "PWD:TGGS_KMUTNB");
        oled_draw_str(&s_oled, 0, 16, "URL:" AP_DNS_NAME);
        oled_draw_str(&s_oled, 0, 24, "IP:192.168.4.1");
        oled_update_display(&s_oled);
        s_ap_screen_drawn = true;
        return;
    }

    s_ap_screen_drawn = false;
    if (!s_oled_prev_valid ||
        s_oled_prev_status.wifi_ok != s_status.wifi_ok ||
        s_oled_prev_status.mqtt_ok != s_status.mqtt_ok ||
        s_oled_prev_status.time_ok != s_status.time_ok ||
        s_oled_prev_status.ap_mode != s_status.ap_mode ||
        s_oled_prev_status.relay_on != s_status.relay_on ||
        s_oled_prev_status.servo_angle != s_status.servo_angle ||
        s_oled_prev_status.led_mode != s_status.led_mode ||
        s_oled_prev_status.buzzer_active != s_status.buzzer_active ||
        strcmp(s_oled_prev_cmd_text, s_last_cmd_text) != 0) {
        oled_show_status_screen();
        s_oled_prev_status = s_status;
        strlcpy_safe(s_oled_prev_cmd_text, sizeof(s_oled_prev_cmd_text), s_last_cmd_text);
        s_oled_prev_valid = true;
    }
}

static void mqtt_publish_availability(const char *state)
{
    if (!s_mqtt_client || !state) {
        return;
    }
    esp_mqtt_client_publish(s_mqtt_client, s_topic_availability, state, 0, 1, 1);
}

static void mqtt_publish_ack(const char *msg_id, bool accepted, const char *code, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    char ts[32];
    char *json;

    if (!root) {
        return;
    }

    format_timestamp_iso(ts, sizeof(ts));

    cJSON_AddNumberToObject(root, "v", MQTT_PROTO_VERSION);
    cJSON_AddStringToObject(root, "msg_id", msg_id ? msg_id : "");
    cJSON_AddStringToObject(root, "device_id", s_device_id);
    cJSON_AddBoolToObject(root, "accepted", accepted);
    cJSON_AddStringToObject(root, "code", code ? code : "OK");
    cJSON_AddStringToObject(root, "message", message ? message : "");
    cJSON_AddStringToObject(root, "ts", ts);

    json = cJSON_PrintUnformatted(root);
    if (json) {
        esp_mqtt_client_publish(s_mqtt_client, s_topic_ack, json, 0, 1, 0);
        cJSON_free(json);
    }
    cJSON_Delete(root);
}

static void mqtt_publish_status(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *net;
    cJSON *actuators;
    cJSON *meta;
    cJSON *led;
    char ts[32];
    char *json;

    wifi_ap_record_t ap_info;
    esp_netif_ip_info_t ip_info;
    bool got_ap = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
    bool got_ip = (s_sta_netif && esp_netif_get_ip_info(s_sta_netif, &ip_info) == ESP_OK);

    if (!root) {
        return;
    }

    format_timestamp_iso(ts, sizeof(ts));

    cJSON_AddNumberToObject(root, "v", MQTT_PROTO_VERSION);
    cJSON_AddStringToObject(root, "device_id", s_device_id);
    cJSON_AddStringToObject(root, "ts", ts);

    net = cJSON_AddObjectToObject(root, "net");
    cJSON_AddStringToObject(net, "wifi", s_status.wifi_ok ? "connected" : "disconnected");
    cJSON_AddStringToObject(net, "mqtt", s_status.mqtt_ok ? "connected" : "disconnected");
    cJSON_AddNumberToObject(net, "rssi", got_ap ? ap_info.rssi : 0);
    if (got_ip) {
        char ipbuf[20];
        snprintf(ipbuf, sizeof(ipbuf), IPSTR, IP2STR(&ip_info.ip));
        cJSON_AddStringToObject(net, "ip", ipbuf);
    } else {
        cJSON_AddStringToObject(net, "ip", "0.0.0.0");
    }

    actuators = cJSON_AddObjectToObject(root, "actuators");
    cJSON *fan = cJSON_AddObjectToObject(actuators, "fan");
    cJSON_AddBoolToObject(fan, "on", s_status.relay_on);

    cJSON *servo = cJSON_AddObjectToObject(actuators, "servo");
    cJSON_AddNumberToObject(servo, "position_deg", s_status.servo_angle);

    led = cJSON_AddObjectToObject(actuators, "led");
    cJSON_AddBoolToObject(led, "red", s_status.led_mode == LED_RED);
    cJSON_AddBoolToObject(led, "yellow", s_status.led_mode == LED_YELLOW);
    cJSON_AddBoolToObject(led, "green", s_status.led_mode == LED_GREEN);

    cJSON *buzzer = cJSON_AddObjectToObject(actuators, "buzzer");
    cJSON_AddBoolToObject(buzzer, "active", s_status.buzzer_active);

    cJSON *oled = cJSON_AddObjectToObject(actuators, "oled");
    cJSON_AddStringToObject(oled, "mode", "status");
    cJSON_AddStringToObject(oled, "last_cmd_text", s_last_cmd_text);

    meta = cJSON_AddObjectToObject(root, "meta");
    cJSON_AddNumberToObject(meta, "uptime_s", (double)(esp_timer_get_time() / 1000000));
    cJSON_AddStringToObject(meta, "last_cmd_id", s_last_cmd_id);
    cJSON_AddStringToObject(meta, "last_error", s_last_error);

    json = cJSON_PrintUnformatted(root);
    if (json) {
        esp_mqtt_client_publish(s_mqtt_client, s_topic_status, json, 0, 1, 1);
        cJSON_free(json);
    }

    cJSON_Delete(root);
}

static void buzzer_beep_pattern(int count, int on_ms, int off_ms)
{
    int i;
    if (count < 1) count = 1;
    if (on_ms < 10) on_ms = 10;
    if (off_ms < 0) off_ms = 0;

    for (i = 0; i < count; i++) {
        s_status.buzzer_active = true;
        buzzer_beep(&s_buzzer, (uint32_t)on_ms);
        s_status.buzzer_active = false;
        if (i != count - 1) {
            vTaskDelay(pdMS_TO_TICKS(off_ms));
        }
    }
}

static bool parse_required_string(cJSON *obj, const char *key, char *out, size_t out_len)
{
    cJSON *n = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsString(n) || !n->valuestring) {
        return false;
    }
    strlcpy_safe(out, out_len, n->valuestring);
    return true;
}

static bool process_command_json(
    const char *payload,
    char *msg_id,
    size_t msg_id_len,
    char *cmd_text,
    size_t cmd_text_len,
    char *err_code,
    size_t err_code_len,
    char *err_msg,
    size_t err_msg_len)
{
    cJSON *root = cJSON_Parse(payload);
    cJSON *params;
    char target[16];
    char action[16];
    cJSON *v;

    strlcpy_safe(msg_id, msg_id_len, "");
    strlcpy_safe(cmd_text, cmd_text_len, "");
    strlcpy_safe(err_code, err_code_len, "OK");
    strlcpy_safe(err_msg, err_msg_len, "Command applied");

    if (!root) {
        strlcpy_safe(err_code, err_code_len, "INVALID_JSON");
        strlcpy_safe(err_msg, err_msg_len, "Payload is not valid JSON");
        return false;
    }

    v = cJSON_GetObjectItemCaseSensitive(root, "v");
    if (!cJSON_IsNumber(v) || v->valueint != MQTT_PROTO_VERSION) {
        strlcpy_safe(err_code, err_code_len, "INVALID_VERSION");
        strlcpy_safe(err_msg, err_msg_len, "Unsupported protocol version");
        cJSON_Delete(root);
        return false;
    }

    if (!parse_required_string(root, "msg_id", msg_id, msg_id_len)) {
        strlcpy_safe(err_code, err_code_len, "MISSING_FIELD");
        strlcpy_safe(err_msg, err_msg_len, "msg_id is required");
        cJSON_Delete(root);
        return false;
    }

    if (strcmp(msg_id, s_last_cmd_id) == 0) {
        strlcpy_safe(err_code, err_code_len, "DUPLICATE");
        strlcpy_safe(err_msg, err_msg_len, "Duplicate msg_id ignored");
        cJSON_Delete(root);
        return false;
    }

    if (!parse_required_string(root, "target", target, sizeof(target)) ||
        !parse_required_string(root, "action", action, sizeof(action))) {
        strlcpy_safe(err_code, err_code_len, "MISSING_FIELD");
        strlcpy_safe(err_msg, err_msg_len, "target and action are required");
        cJSON_Delete(root);
        return false;
    }

    params = cJSON_GetObjectItemCaseSensitive(root, "params");
    if (params && !cJSON_IsObject(params)) {
        params = NULL;
    }

    if (strcmp(target, "fan") == 0 && strcmp(action, "set") == 0) {
        cJSON *on = params ? cJSON_GetObjectItemCaseSensitive(params, "on") : NULL;
        if (!cJSON_IsBool(on)) {
            strlcpy_safe(err_code, err_code_len, "INVALID_PARAM");
            strlcpy_safe(err_msg, err_msg_len, "fan params.on must be bool");
            cJSON_Delete(root);
            return false;
        }
        s_status.relay_on = cJSON_IsTrue(on);
        snprintf(cmd_text, cmd_text_len, "fan %s", s_status.relay_on ? "on" : "off");
    } else if (strcmp(target, "servo") == 0 && strcmp(action, "set") == 0) {
        cJSON *p = params ? cJSON_GetObjectItemCaseSensitive(params, "position_deg") : NULL;
        if (!cJSON_IsNumber(p) || p->valueint < 0 || p->valueint > 180) {
            strlcpy_safe(err_code, err_code_len, "INVALID_PARAM");
            strlcpy_safe(err_msg, err_msg_len, "position_deg out of range (0..180)");
            cJSON_Delete(root);
            return false;
        }
        s_status.servo_angle = p->valueint;
        snprintf(cmd_text, cmd_text_len, "servo %d", s_status.servo_angle);
    } else if (strcmp(target, "led") == 0 && strcmp(action, "set") == 0) {
        cJSON *color = params ? cJSON_GetObjectItemCaseSensitive(params, "color") : NULL;
        if (cJSON_IsString(color) && color->valuestring) {
            if (strcmp(color->valuestring, "red") == 0) {
                s_status.led_mode = LED_RED;
            } else if (strcmp(color->valuestring, "yellow") == 0) {
                s_status.led_mode = LED_YELLOW;
            } else if (strcmp(color->valuestring, "green") == 0) {
                s_status.led_mode = LED_GREEN;
            } else if (strcmp(color->valuestring, "off") == 0) {
                s_status.led_mode = LED_MODE_OFF;
            } else {
                strlcpy_safe(err_code, err_code_len, "INVALID_PARAM");
                strlcpy_safe(err_msg, err_msg_len, "led color must be red/yellow/green/off");
                cJSON_Delete(root);
                return false;
            }
        } else {
            cJSON *r = params ? cJSON_GetObjectItemCaseSensitive(params, "red") : NULL;
            cJSON *y = params ? cJSON_GetObjectItemCaseSensitive(params, "yellow") : NULL;
            cJSON *g = params ? cJSON_GetObjectItemCaseSensitive(params, "green") : NULL;
            if ((r && !cJSON_IsBool(r)) || (y && !cJSON_IsBool(y)) || (g && !cJSON_IsBool(g))) {
                strlcpy_safe(err_code, err_code_len, "INVALID_PARAM");
                strlcpy_safe(err_msg, err_msg_len, "led red/yellow/green must be bool");
                cJSON_Delete(root);
                return false;
            }
            if (r && cJSON_IsTrue(r)) {
                s_status.led_mode = LED_RED;
            } else if (y && cJSON_IsTrue(y)) {
                s_status.led_mode = LED_YELLOW;
            } else if (g && cJSON_IsTrue(g)) {
                s_status.led_mode = LED_GREEN;
            } else {
                s_status.led_mode = LED_MODE_OFF;
            }
        }
        if (s_status.led_mode == LED_RED) {
            strlcpy_safe(cmd_text, cmd_text_len, "led red");
        } else if (s_status.led_mode == LED_YELLOW) {
            strlcpy_safe(cmd_text, cmd_text_len, "led yellow");
        } else if (s_status.led_mode == LED_GREEN) {
            strlcpy_safe(cmd_text, cmd_text_len, "led green");
        } else {
            strlcpy_safe(cmd_text, cmd_text_len, "led off");
        }
    } else if (strcmp(target, "buzzer") == 0 && strcmp(action, "beep") == 0) {
        cJSON *count = params ? cJSON_GetObjectItemCaseSensitive(params, "count") : NULL;
        cJSON *on_ms = params ? cJSON_GetObjectItemCaseSensitive(params, "on_ms") : NULL;
        cJSON *off_ms = params ? cJSON_GetObjectItemCaseSensitive(params, "off_ms") : NULL;
        int c = cJSON_IsNumber(count) ? count->valueint : 1;
        int on = cJSON_IsNumber(on_ms) ? on_ms->valueint : 100;
        int off = cJSON_IsNumber(off_ms) ? off_ms->valueint : 100;

        if (c < 1 || on < 10 || on > 5000 || off < 0 || off > 5000) {
            strlcpy_safe(err_code, err_code_len, "INVALID_PARAM");
            strlcpy_safe(err_msg, err_msg_len, "buzzer params invalid");
            cJSON_Delete(root);
            return false;
        }
        buzzer_beep_pattern(c, on, off);
        snprintf(cmd_text, cmd_text_len, "buzzer %dx", c);
    } else if (strcmp(target, "oled") == 0 && strcmp(action, "show") == 0) {
        const cJSON *l1 = params ? cJSON_GetObjectItemCaseSensitive(params, "line1") : NULL;
        const cJSON *l2 = params ? cJSON_GetObjectItemCaseSensitive(params, "line2") : NULL;
        const cJSON *l3 = params ? cJSON_GetObjectItemCaseSensitive(params, "line3") : NULL;
        const cJSON *l4 = params ? cJSON_GetObjectItemCaseSensitive(params, "line4") : NULL;
        oled_show_custom(
            cJSON_IsString(l1) ? l1->valuestring : "",
            cJSON_IsString(l2) ? l2->valuestring : "",
            cJSON_IsString(l3) ? l3->valuestring : "",
            cJSON_IsString(l4) ? l4->valuestring : "",
            pdMS_TO_TICKS(OLED_CUSTOM_SHOW_MS));
        strlcpy_safe(cmd_text, cmd_text_len, "oled show");
    } else if (strcmp(target, "system") == 0 && strcmp(action, "get_status") == 0) {
        strlcpy_safe(cmd_text, cmd_text_len, "system status");
    } else {
        strlcpy_safe(err_code, err_code_len, "UNSUPPORTED");
        strlcpy_safe(err_msg, err_msg_len, "Unsupported target/action");
        cJSON_Delete(root);
        return false;
    }

    cJSON_Delete(root);
    return true;
}

static void mqtt_handle_command(const char *payload, size_t payload_len)
{
    char msg_id[MSG_ID_LEN];
    char cmd_text[LAST_CMD_TEXT_LEN];
    char err_code[32];
    char err_msg[96];
    char payload_buf[512];
    bool accepted;

    if (payload_len >= sizeof(payload_buf)) {
        payload_len = sizeof(payload_buf) - 1;
    }
    memcpy(payload_buf, payload, payload_len);
    payload_buf[payload_len] = '\0';

    accepted = process_command_json(
        payload_buf,
        msg_id,
        sizeof(msg_id),
        cmd_text,
        sizeof(cmd_text),
        err_code,
        sizeof(err_code),
        err_msg,
        sizeof(err_msg));

    if (accepted) {
        strlcpy_safe(s_last_cmd_id, sizeof(s_last_cmd_id), msg_id);
        strlcpy_safe(s_last_cmd_text, sizeof(s_last_cmd_text), cmd_text);
        s_last_error[0] = '\0';

        apply_actuator_state();
        oled_show_cmd_received(cmd_text);
        mqtt_publish_ack(msg_id, true, "OK", "Command applied");
        mqtt_publish_status();
    } else {
        if (msg_id[0] != '\0') {
            mqtt_publish_ack(msg_id, false, err_code, err_msg);
        }
        strlcpy_safe(s_last_error, sizeof(s_last_error), err_msg);
    }

    oled_render();
}

static void mqtt_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    (void)arg;
    (void)event_base;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            s_status.mqtt_ok = true;
            xEventGroupSetBits(s_evt_group, MQTT_CONNECTED_BIT);
            esp_mqtt_client_subscribe(s_mqtt_client, s_topic_cmd, 1);
            mqtt_publish_availability("online");
            mqtt_publish_status();
            ESP_LOGI(TAG, "MQTT connected, subscribed to %s", s_topic_cmd);
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_status.mqtt_ok = false;
            xEventGroupClearBits(s_evt_group, MQTT_CONNECTED_BIT);
            ESP_LOGW(TAG, "MQTT disconnected");
            break;
        case MQTT_EVENT_DATA:
            mqtt_handle_command(event->data, (size_t)event->data_len);
            break;
        default:
            break;
    }
}

static size_t dns_skip_qname(const uint8_t *packet, size_t packet_len, size_t offset)
{
    while (offset < packet_len) {
        uint8_t label_len = packet[offset];
        if (label_len == 0) {
            return offset + 1;
        }
        if ((label_len & 0xC0) == 0xC0) {
            if (offset + 1 < packet_len) {
                return offset + 2;
            }
            return 0;
        }
        offset += (size_t)label_len + 1;
        if (offset > packet_len) {
            return 0;
        }
    }
    return 0;
}

static void dns_server_task(void *arg)
{
    struct sockaddr_in bind_addr;
    struct timeval tv = {
        .tv_sec = 1,
        .tv_usec = 0,
    };
    (void)arg;

    s_dns_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_dns_sock < 0) {
        s_dns_running = false;
        s_dns_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    setsockopt(s_dns_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(DNS_SERVER_PORT);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s_dns_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        lwip_close(s_dns_sock);
        s_dns_sock = -1;
        s_dns_running = false;
        s_dns_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    while (s_dns_running) {
        uint8_t req[512];
        uint8_t resp[512];
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        int n = recvfrom(s_dns_sock, req, sizeof(req), 0, (struct sockaddr *)&from_addr, &from_len);

        if (n < 12) {
            continue;
        }

        {
            uint16_t qdcount = (uint16_t)((req[4] << 8) | req[5]);
            size_t qname_end;
            size_t question_end;
            uint16_t qtype;
            uint16_t qclass;
            size_t resp_len;

            if (qdcount == 0) {
                continue;
            }

            qname_end = dns_skip_qname(req, (size_t)n, 12);
            if (qname_end == 0 || qname_end + 4 > (size_t)n) {
                continue;
            }
            question_end = qname_end + 4;

            qtype = (uint16_t)((req[qname_end] << 8) | req[qname_end + 1]);
            qclass = (uint16_t)((req[qname_end + 2] << 8) | req[qname_end + 3]);

            memset(resp, 0, sizeof(resp));
            resp[0] = req[0];
            resp[1] = req[1];
            resp[2] = 0x81;
            resp[3] = 0x80;
            resp[4] = 0x00;
            resp[5] = 0x01;

            memcpy(resp + 12, req + 12, question_end - 12);
            resp_len = question_end;

            if (qclass == 1 && (qtype == 1 || qtype == 28 || qtype == 255)) {
                uint32_t ip = inet_addr(AP_IP_ADDR);
                uint8_t ip6_mapped[16] = {
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0xFF, 0xFF,
                    0xC0, 0xA8, 0x04, 0x01,
                };

                if (qtype == 28 && resp_len + 28 > sizeof(resp)) {
                    continue;
                }
                if ((qtype == 1 || qtype == 255) && resp_len + 16 > sizeof(resp)) {
                    continue;
                }

                resp[6] = 0x00;
                resp[7] = 0x01;

                resp[resp_len++] = 0xC0;
                resp[resp_len++] = 0x0C;
                resp[resp_len++] = 0x00;
                resp[resp_len++] = (qtype == 28) ? 0x1C : 0x01;
                resp[resp_len++] = 0x00;
                resp[resp_len++] = 0x01;
                resp[resp_len++] = 0x00;
                resp[resp_len++] = 0x00;
                resp[resp_len++] = 0x00;
                resp[resp_len++] = 0x3C;
                if (qtype == 28) {
                    resp[resp_len++] = 0x00;
                    resp[resp_len++] = 0x10;
                    memcpy(resp + resp_len, ip6_mapped, 16);
                    resp_len += 16;
                } else {
                    resp[resp_len++] = 0x00;
                    resp[resp_len++] = 0x04;
                    memcpy(resp + resp_len, &ip, 4);
                    resp_len += 4;
                }
            }

            sendto(s_dns_sock, resp, (int)resp_len, 0, (struct sockaddr *)&from_addr, from_len);
        }
    }

    if (s_dns_sock >= 0) {
        lwip_close(s_dns_sock);
        s_dns_sock = -1;
    }
    s_dns_task_handle = NULL;
    vTaskDelete(NULL);
}

static void start_dns_server(void)
{
    if (s_dns_running) {
        return;
    }
    s_dns_running = true;
    ESP_LOGI(TAG, "Starting wildcard DNS server on %s:%d", AP_IP_ADDR, DNS_SERVER_PORT);
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 4, &s_dns_task_handle);
}

static void stop_dns_server(void)
{
    if (!s_dns_running) {
        return;
    }
    s_dns_running = false;
    ESP_LOGI(TAG, "Stopping wildcard DNS server");
    if (s_dns_sock >= 0) {
        lwip_close(s_dns_sock);
        s_dns_sock = -1;
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_status.wifi_ok = false;
        xEventGroupClearBits(s_evt_group, WIFI_CONNECTED_BIT);
        if (!s_status.ap_mode) {
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_status.wifi_ok = true;
        xEventGroupSetBits(s_evt_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_start_sta(void)
{
    wifi_config_t wifi_cfg = {0};

    strlcpy_safe((char *)wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid), s_cfg.wifi_ssid);
    strlcpy_safe((char *)wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password), s_cfg.wifi_pass);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    stop_dns_server();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void wifi_start_ap(void)
{
    wifi_config_t ap_cfg = {0};

    strlcpy_safe((char *)ap_cfg.ap.ssid, sizeof(ap_cfg.ap.ssid), AP_SSID);
    ap_cfg.ap.ssid_len = strlen(AP_SSID);
    strlcpy_safe((char *)ap_cfg.ap.password, sizeof(ap_cfg.ap.password), AP_PASS);
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    start_dns_server();

    s_status.ap_mode = true;
    s_status.wifi_ok = true;
    s_status.mqtt_ok = false;
    s_status.time_ok = false;
}

static bool config_load(app_config_t *cfg)
{
    nvs_handle_t nvs;
    size_t sz = sizeof(*cfg);
    esp_err_t err;

    memset(cfg, 0, sizeof(*cfg));
    err = nvs_open("appcfg", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_get_blob(nvs, "cfg", cfg, &sz);
    nvs_close(nvs);
    return (err == ESP_OK && cfg->valid == 1);
}

static esp_err_t config_save(const app_config_t *cfg)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("appcfg", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(nvs, "cfg", cfg, sizeof(*cfg));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

static void url_decode(char *dst, size_t dst_len, const char *src)
{
    size_t di = 0;

    while (*src && di + 1 < dst_len) {
        if (*src == '+') {
            dst[di++] = ' ';
            src++;
        } else if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            char hex[3] = {src[1], src[2], '\0'};
            dst[di++] = (char)strtol(hex, NULL, 16);
            src += 3;
        } else {
            dst[di++] = *src++;
        }
    }
    dst[di] = '\0';
}

static void cfg_set_field(char *dst, size_t dst_sz, const char *v)
{
    strlcpy_safe(dst, dst_sz, v);
}

static void cfg_parse_form(app_config_t *cfg, char *body)
{
    char *pair = strtok(body, "&");
    while (pair) {
        char *eq = strchr(pair, '=');
        if (eq) {
            char key[32];
            char val[160];

            *eq = '\0';
            url_decode(key, sizeof(key), pair);
            url_decode(val, sizeof(val), eq + 1);

            if (strcmp(key, "wifi_ssid") == 0) cfg_set_field(cfg->wifi_ssid, sizeof(cfg->wifi_ssid), val);
            else if (strcmp(key, "wifi_pass") == 0) cfg_set_field(cfg->wifi_pass, sizeof(cfg->wifi_pass), val);
            else if (strcmp(key, "mqtt_uri") == 0) cfg_set_field(cfg->mqtt_uri, sizeof(cfg->mqtt_uri), val);
            else if (strcmp(key, "mqtt_user") == 0) cfg_set_field(cfg->mqtt_user, sizeof(cfg->mqtt_user), val);
            else if (strcmp(key, "mqtt_pass") == 0) cfg_set_field(cfg->mqtt_pass, sizeof(cfg->mqtt_pass), val);
            else if (strcmp(key, "mqtt_topic") == 0) cfg_set_field(cfg->mqtt_topic, sizeof(cfg->mqtt_topic), val);
            else if (strcmp(key, "ntp_server") == 0) cfg_set_field(cfg->ntp_server, sizeof(cfg->ntp_server), val);
        }
        pair = strtok(NULL, "&");
    }
}

static esp_err_t http_root_get(httpd_req_t *req)
{
    char html[1800];

    snprintf(
        html,
        sizeof(html),
        "<!doctype html><html><head><meta charset='utf-8'><title>IOT Config</title></head><body>"
        "<h2>IOT ACTUATOR DEMO CONFIG</h2>"
        "<p>Device ID: %s</p>"
        "<p>Connect to AP then open <b>http://" AP_DNS_NAME "</b> (or any domain) in browser.</p>"
        "<form method='post' action='/save'>"
        "WIFI SSID:<br><input name='wifi_ssid' required><br>"
        "WIFI PASS:<br><input name='wifi_pass' type='password'><br>"
        "MQTT URI:<br><input name='mqtt_uri' value='mqtt://192.168.1.2:1883' required><br>"
        "MQTT USER:<br><input name='mqtt_user'><br>"
        "MQTT PASS:<br><input name='mqtt_pass' type='password'><br>"
        "MQTT CMD TOPIC:<br><input name='mqtt_topic' value='iot/actuator/{deviceId}/cmd' required><br>"
        "NTP SERVER:<br><input name='ntp_server' value='pool.ntp.org' required><br><br>"
        "<button type='submit'>SAVE AND RESTART</button>"
        "</form></body></html>",
        s_device_id);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t http_captive_redirect_get(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://" AP_DNS_NAME "/");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "Redirecting to configuration portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t http_save_post(httpd_req_t *req)
{
    char body[700];
    int recv_len;
    app_config_t cfg = {0};

    if (req->content_len >= (int)sizeof(body)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "FORM TOO LARGE");
        return ESP_FAIL;
    }

    recv_len = httpd_req_recv(req, body, req->content_len);
    if (recv_len <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "FAILED TO READ BODY");
        return ESP_FAIL;
    }
    body[recv_len] = '\0';

    cfg_parse_form(&cfg, body);
    if (cfg.ntp_server[0] == '\0') {
        cfg_set_field(cfg.ntp_server, sizeof(cfg.ntp_server), "pool.ntp.org");
    }
    cfg.valid = 1;

    if (cfg.wifi_ssid[0] == '\0' || cfg.mqtt_uri[0] == '\0' || cfg.mqtt_topic[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "MISSING REQUIRED FIELDS");
        return ESP_FAIL;
    }

    if (config_save(&cfg) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SAVE FAILED");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "CONFIG SAVED. RESTARTING...", HTTPD_RESP_USE_STRLEN);
    s_reboot_requested = true;
    return ESP_OK;
}

static void start_config_webserver(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = http_root_get,
    };
    httpd_uri_t save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = http_save_post,
    };
    httpd_uri_t captive_generate_204 = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = http_captive_redirect_get,
    };
    httpd_uri_t captive_hotspot_detect = {
        .uri = "/hotspot-detect.html",
        .method = HTTP_GET,
        .handler = http_captive_redirect_get,
    };
    httpd_uri_t wildcard = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = http_root_get,
    };

    if (httpd_start(&s_httpd, &cfg) == ESP_OK) {
        httpd_register_uri_handler(s_httpd, &root);
        httpd_register_uri_handler(s_httpd, &save);
        httpd_register_uri_handler(s_httpd, &captive_generate_204);
        httpd_register_uri_handler(s_httpd, &captive_hotspot_detect);
        httpd_register_uri_handler(s_httpd, &wildcard);
        ESP_LOGI(TAG, "Config web server started: http://%s", AP_DNS_NAME);
    }
}

static bool sync_time_ntp(const char *server)
{
    time_t now;
    struct tm info;

    esp_sntp_stop();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, (char *)(server && server[0] ? server : "pool.ntp.org"));
    esp_sntp_init();

    for (int i = 0; i < 20; ++i) {
        time(&now);
        localtime_r(&now, &info);
        if (info.tm_year >= (2024 - 1900)) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    return false;
}

static bool try_connect_runtime(void)
{
    EventBits_t bits;
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = s_cfg.mqtt_uri,
        .credentials.username = s_cfg.mqtt_user,
        .credentials.authentication.password = s_cfg.mqtt_pass,
        .session.last_will.topic = s_topic_availability,
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };

    s_status.ap_mode = false;
    wifi_start_sta();

    bits = xEventGroupWaitBits(s_evt_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    if ((bits & WIFI_CONNECTED_BIT) == 0) {
        ESP_LOGW(TAG, "WiFi connection timeout");
        return false;
    }

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);

    bits = xEventGroupWaitBits(s_evt_group, MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(MQTT_CONNECT_TIMEOUT_MS));
    if ((bits & MQTT_CONNECTED_BIT) == 0) {
        ESP_LOGW(TAG, "MQTT connection timeout");
        return false;
    }

    s_status.time_ok = sync_time_ntp(s_cfg.ntp_server);
    mqtt_publish_status();
    return true;
}

static void show_splash(void)
{
    oled_draw_bitmap_full(&s_oled, ece_logo_128x32);
    vTaskDelay(pdMS_TO_TICKS(1000));

    oled_clear(&s_oled);
    oled_draw_str_big(&s_oled, 42, 0, "TGGS");
    oled_draw_str_big(&s_oled, 31, 16, "KMUTNB");
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void app_main(void)
{
    TickType_t next_status_publish = 0;

    ESP_LOGI(TAG, "ESP32 Actuator Demo starting");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_evt_group = xEventGroupCreate();
    build_device_id();

    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    (void)s_ap_netif;

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    oled_init(&s_oled);

    s_buzzer.gpio = BUZZER_GPIO;
    s_buzzer.state = 0;
    buzzer_init(&s_buzzer);

    s_led_red.gpio = LED_RED_GPIO;
    s_led_red.state = 0;
    s_led_yellow.gpio = LED_YELLOW_GPIO;
    s_led_yellow.state = 0;
    s_led_green.gpio = LED_GREEN_GPIO;
    s_led_green.state = 0;
    led_traffic_init(&s_led_red, &s_led_yellow, &s_led_green);

    s_relay.gpio = RELAY_GPIO;
    s_relay.active_level = RELAY_ACTIVE_LEVEL;
    s_relay.state = 0;
    relay_init(&s_relay);

    s_servo.gpio = SERVO_GPIO;
    servo_init(&s_servo);

    s_button.gpio = BUTTON_GPIO;
    s_button.active_level = BUTTON_ACTIVE_LEVEL;
    s_button.debounce_ms = BUTTON_DEBOUNCE_MS;
    s_button.last_press_time = 0;
    s_button.last_state = 1;
    button_init(&s_button);

    show_splash();

    post_result_t post_result;
    post_init(&post_result);
    post_run_all(&post_result, &s_oled);

    s_status.relay_on = false;
    s_status.servo_angle = 0;
    s_status.led_mode = LED_GREEN;
    s_status.buzzer_active = false;
    apply_actuator_state();

    if (!config_load(&s_cfg)) {
        strlcpy_safe(s_cfg.mqtt_topic, sizeof(s_cfg.mqtt_topic), "iot/actuator/{deviceId}/cmd");
    }
    build_topics();

    if (s_cfg.valid && try_connect_runtime()) {
        ESP_LOGI(TAG, "Runtime connected with stored config");
    } else {
        ESP_LOGW(TAG, "Using config AP mode");
        if (s_mqtt_client) {
            mqtt_publish_availability("offline");
            esp_mqtt_client_stop(s_mqtt_client);
            esp_mqtt_client_destroy(s_mqtt_client);
            s_mqtt_client = NULL;
        }
        esp_wifi_stop();
        wifi_start_ap();
        start_config_webserver();
    }

    next_status_publish = xTaskGetTickCount() + pdMS_TO_TICKS(30000);

    while (1) {
        if (s_reboot_requested) {
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        }

        if (s_status.ap_mode) {
            if (button_is_pressed(&s_button)) {
                buzzer_beep_pattern(1, 30, 0);
            }
        } else if (s_status.mqtt_ok && xTaskGetTickCount() >= next_status_publish) {
            mqtt_publish_status();
            next_status_publish = xTaskGetTickCount() + pdMS_TO_TICKS(30000);
        }

        oled_render();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
