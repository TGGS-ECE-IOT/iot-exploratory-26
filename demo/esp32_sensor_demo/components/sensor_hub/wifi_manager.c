#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"

#include "lwip/sockets.h"

#include "nvs.h"

#include "driver/gpio.h"

#include "app_state.h"
#include "mqtt_manager.h"
#include "pin_config.h"
#include "status.h"
#include "wifi_manager.h"

#define TAG "IOT_WIFI"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAX_RETRY     20

#define CONFIG_AP_PASS "tggs_kmutnb"
#define CFG_NS         "node_cfg"

static httpd_handle_t s_http_server;
static bool s_wifi_initialized;
static bool s_wifi_started;
static bool s_handlers_registered;

static TaskHandle_t s_dns_task;
static int s_dns_sock = -1;
static volatile bool s_dns_running;

static uint16_t dns_rd_u16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void dns_wr_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)((v >> 8) & 0xFF);
    p[1] = (uint8_t)(v & 0xFF);
}

static int dns_build_response(const uint8_t *req, int req_len, uint8_t *resp, int resp_len) {
    static const uint8_t ap_ip[4] = {192, 168, 4, 1};
    const int dns_header_len = 12;

    if (req == NULL || resp == NULL || req_len < dns_header_len || resp_len < 32) {
        return -1;
    }

    uint16_t qdcount = dns_rd_u16(&req[4]);
    if (qdcount == 0) {
        return -1;
    }

    int qname_end = dns_header_len;
    while (qname_end < req_len && req[qname_end] != 0) {
        qname_end += req[qname_end] + 1;
    }
    if (qname_end + 5 > req_len) {
        return -1;
    }

    int q_end = qname_end + 1 + 4;
    uint16_t qtype = dns_rd_u16(&req[qname_end + 1]);
    uint16_t qclass = dns_rd_u16(&req[qname_end + 3]);

    if (q_end + 16 > resp_len) {
        return -1;
    }

    memset(resp, 0, (size_t)resp_len);
    memcpy(resp, req, (size_t)q_end);

    resp[2] = 0x81;
    resp[3] = 0x80;
    dns_wr_u16(&resp[4], 1);
    dns_wr_u16(&resp[6], (qclass == 1 && (qtype == 1 || qtype == 255)) ? 1 : 0);
    dns_wr_u16(&resp[8], 0);
    dns_wr_u16(&resp[10], 0);

    if (!(qclass == 1 && (qtype == 1 || qtype == 255))) {
        return q_end;
    }

    int p = q_end;
    resp[p++] = 0xC0;
    resp[p++] = 0x0C;
    dns_wr_u16(&resp[p], 1);
    p += 2;
    dns_wr_u16(&resp[p], 1);
    p += 2;
    resp[p++] = 0x00;
    resp[p++] = 0x00;
    resp[p++] = 0x00;
    resp[p++] = 0x3C;
    dns_wr_u16(&resp[p], 4);
    p += 2;
    memcpy(&resp[p], ap_ip, 4);
    p += 4;

    return p;
}

static void captive_dns_task(void *arg) {
    (void)arg;

    uint8_t req[512];
    uint8_t resp[512];

    while (s_dns_running) {
        struct sockaddr_in src_addr;
        socklen_t src_len = sizeof(src_addr);
        int r = recvfrom(s_dns_sock, req, sizeof(req), 0, (struct sockaddr *)&src_addr, &src_len);
        if (r < 0) {
            continue;
        }

        int out_len = dns_build_response(req, r, resp, sizeof(resp));
        if (out_len > 0) {
            sendto(s_dns_sock, resp, out_len, 0, (struct sockaddr *)&src_addr, src_len);
        }
    }

    vTaskDelete(NULL);
}

static void captive_dns_start(void) {
    if (s_dns_running) {
        return;
    }

    s_dns_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_dns_sock < 0) {
        ESP_LOGW(TAG, "DNS socket create failed");
        return;
    }

    struct timeval tv = {
        .tv_sec = 1,
        .tv_usec = 0,
    };
    setsockopt(s_dns_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(s_dns_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGW(TAG, "DNS bind port 53 failed");
        close(s_dns_sock);
        s_dns_sock = -1;
        return;
    }

    s_dns_running = true;
    if (xTaskCreate(captive_dns_task, "captive_dns", 3072, NULL, 4, &s_dns_task) != pdPASS) {
        s_dns_running = false;
        close(s_dns_sock);
        s_dns_sock = -1;
        ESP_LOGW(TAG, "DNS task create failed");
        return;
    }
}

static void captive_dns_stop(void) {
    if (!s_dns_running) {
        return;
    }

    s_dns_running = false;
    if (s_dns_sock >= 0) {
        shutdown(s_dns_sock, 0);
        close(s_dns_sock);
        s_dns_sock = -1;
    }
    s_dns_task = NULL;
}

static const char *CONFIG_HTML =
    "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>TGGS Sensor Node Config</title><style>body{font-family:Arial,sans-serif;margin:20px;background:#f4f6f8}"
    ".card{max-width:560px;background:#fff;padding:20px;border-radius:12px;box-shadow:0 6px 20px rgba(0,0,0,.08)}"
    "h2{margin-top:0}label{font-weight:600;font-size:13px}input{width:100%;padding:10px;margin:6px 0 12px;border:1px solid #cdd6df;border-radius:8px}"
    "button{width:100%;padding:12px;background:#0069c2;color:#fff;border:0;border-radius:8px;font-weight:700}"
    "small{color:#56606b}</style></head><body><div class='card'><h2>TGGS Sensor Node Setup</h2>"
    "<form method='POST' action='/connect'>"
    "<label>Wi-Fi SSID</label><input name='wifi_ssid' maxlength='32' required>"
    "<label>Wi-Fi Password</label><input name='wifi_password' maxlength='64' type='password'>"
    "<label>MQTT Host (RPi)</label><input name='mqtt_host' maxlength='63' required>"
    "<label>NTP Host (RPi)</label><input name='ntp_host' maxlength='63' value='192.168.50.1' required>"
    "<label>MQTT Port</label><input name='mqtt_port' maxlength='5' value='1883'>"
    "<label>MQTT Username</label><input name='mqtt_username' maxlength='32'>"
    "<label>MQTT Password</label><input name='mqtt_password' maxlength='64' type='password'>"
    "<label>Topic Base</label><input name='topic_base' maxlength='32' value='tggs/v1'>"
    "<label>Site ID</label><input name='site_id' maxlength='32' value='demo-site'>"
    "<label>Node ID</label><input name='node_id' maxlength='32' value='sensor-001'>"
    "<label>Node Type</label><input name='node_type' maxlength='16' value='sensor'>"
    "<label>Publish Interval ms</label><input name='pub_interval_ms' maxlength='6' value='2000'>"
    "<button type='submit'>Save and Connect</button></form>"
    "<p><small>Config AP: TGGS_SENSOR_NODE / tggs_kmutnb</small></p></div></body></html>";

static void sntp_start_from_rpi(void) {
    esp_sntp_stop();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, g_ntp_host[0] ? g_ntp_host : g_mqtt_host);
    esp_sntp_init();
}

typedef struct {
    char wifi_ssid[33];
    char wifi_password[65];
    char mqtt_host[64];
    char ntp_host[64];
    int mqtt_port;
    char mqtt_username[33];
    char mqtt_password[65];
    char topic_base[33];
    char site_id[33];
    char node_id[33];
    char node_type[17];
    int pub_interval_ms;
} node_cfg_t;

static void cfg_defaults(node_cfg_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->mqtt_port = 1883;
    cfg->pub_interval_ms = 2000;
    strncpy(cfg->mqtt_host, "192.168.50.1", sizeof(cfg->mqtt_host) - 1);
    strncpy(cfg->ntp_host, "192.168.50.1", sizeof(cfg->ntp_host) - 1);
    strncpy(cfg->topic_base, "tggs/v1", sizeof(cfg->topic_base) - 1);
    strncpy(cfg->site_id, "demo-site", sizeof(cfg->site_id) - 1);
    strncpy(cfg->node_id, "sensor-001", sizeof(cfg->node_id) - 1);
    strncpy(cfg->node_type, "sensor", sizeof(cfg->node_type) - 1);
}

static void cfg_apply_globals(const node_cfg_t *cfg) {
    strncpy(g_sta_target_ssid, cfg->wifi_ssid[0] ? cfg->wifi_ssid : "<not set>", sizeof(g_sta_target_ssid) - 1);
    g_sta_target_ssid[sizeof(g_sta_target_ssid) - 1] = '\0';
    strncpy(g_sta_password, cfg->wifi_password, sizeof(g_sta_password) - 1);
    g_sta_password[sizeof(g_sta_password) - 1] = '\0';
    strncpy(g_mqtt_host, cfg->mqtt_host, sizeof(g_mqtt_host) - 1);
    g_mqtt_host[sizeof(g_mqtt_host) - 1] = '\0';
    strncpy(g_ntp_host, cfg->ntp_host[0] ? cfg->ntp_host : cfg->mqtt_host, sizeof(g_ntp_host) - 1);
    g_ntp_host[sizeof(g_ntp_host) - 1] = '\0';
    g_mqtt_port = cfg->mqtt_port;
    strncpy(g_mqtt_username, cfg->mqtt_username, sizeof(g_mqtt_username) - 1);
    g_mqtt_username[sizeof(g_mqtt_username) - 1] = '\0';
    strncpy(g_mqtt_password, cfg->mqtt_password, sizeof(g_mqtt_password) - 1);
    g_mqtt_password[sizeof(g_mqtt_password) - 1] = '\0';
    strncpy(g_topic_base, cfg->topic_base, sizeof(g_topic_base) - 1);
    g_topic_base[sizeof(g_topic_base) - 1] = '\0';
    strncpy(g_site_id, cfg->site_id, sizeof(g_site_id) - 1);
    g_site_id[sizeof(g_site_id) - 1] = '\0';
    strncpy(g_node_id, cfg->node_id, sizeof(g_node_id) - 1);
    g_node_id[sizeof(g_node_id) - 1] = '\0';
    strncpy(g_node_type, cfg->node_type, sizeof(g_node_type) - 1);
    g_node_type[sizeof(g_node_type) - 1] = '\0';
    g_pub_interval_ms = cfg->pub_interval_ms;
}

static esp_err_t cfg_save(const node_cfg_t *cfg) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(CFG_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_str(h, "wifi_ssid", cfg->wifi_ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "wifi_pass", cfg->wifi_password);
    if (err == ESP_OK) err = nvs_set_str(h, "mqtt_host", cfg->mqtt_host);
    if (err == ESP_OK) err = nvs_set_str(h, "ntp_host", cfg->ntp_host);
    if (err == ESP_OK) err = nvs_set_i32(h, "mqtt_port", cfg->mqtt_port);
    if (err == ESP_OK) err = nvs_set_str(h, "mqtt_user", cfg->mqtt_username);
    if (err == ESP_OK) err = nvs_set_str(h, "mqtt_pass", cfg->mqtt_password);
    if (err == ESP_OK) err = nvs_set_str(h, "topic_base", cfg->topic_base);
    if (err == ESP_OK) err = nvs_set_str(h, "site_id", cfg->site_id);
    if (err == ESP_OK) err = nvs_set_str(h, "node_id", cfg->node_id);
    if (err == ESP_OK) err = nvs_set_str(h, "node_type", cfg->node_type);
    if (err == ESP_OK) err = nvs_set_i32(h, "pub_int", cfg->pub_interval_ms);

    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

static esp_err_t cfg_load(node_cfg_t *cfg) {
    nvs_handle_t h;
    cfg_defaults(cfg);

    esp_err_t err = nvs_open(CFG_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    size_t n;
    n = sizeof(cfg->wifi_ssid); nvs_get_str(h, "wifi_ssid", cfg->wifi_ssid, &n);
    n = sizeof(cfg->wifi_password); nvs_get_str(h, "wifi_pass", cfg->wifi_password, &n);
    n = sizeof(cfg->mqtt_host); nvs_get_str(h, "mqtt_host", cfg->mqtt_host, &n);
    n = sizeof(cfg->ntp_host); nvs_get_str(h, "ntp_host", cfg->ntp_host, &n);
    n = sizeof(cfg->mqtt_username); nvs_get_str(h, "mqtt_user", cfg->mqtt_username, &n);
    n = sizeof(cfg->mqtt_password); nvs_get_str(h, "mqtt_pass", cfg->mqtt_password, &n);
    n = sizeof(cfg->topic_base); nvs_get_str(h, "topic_base", cfg->topic_base, &n);
    n = sizeof(cfg->site_id); nvs_get_str(h, "site_id", cfg->site_id, &n);
    n = sizeof(cfg->node_id); nvs_get_str(h, "node_id", cfg->node_id, &n);
    n = sizeof(cfg->node_type); nvs_get_str(h, "node_type", cfg->node_type, &n);

    int32_t i32;
    if (nvs_get_i32(h, "mqtt_port", &i32) == ESP_OK) cfg->mqtt_port = (int)i32;
    if (nvs_get_i32(h, "pub_int", &i32) == ESP_OK) cfg->pub_interval_ms = (int)i32;

    nvs_close(h);
    return ESP_OK;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static void url_decode(char *s) {
    char *src = s;
    char *dst = s;
    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (*src == '%' && src[1] && src[2]) {
            int hi = hex_val(src[1]);
            int lo = hex_val(src[2]);
            if (hi >= 0 && lo >= 0) {
                *dst++ = (char)((hi << 4) | lo);
                src += 3;
            } else {
                *dst++ = *src++;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static void parse_form_field(const char *body, const char *key, char *out, size_t out_len) {
    const char *p = strstr(body, key);
    if (p == NULL) {
        out[0] = '\0';
        return;
    }

    p += strlen(key);
    if (*p != '=') {
        out[0] = '\0';
        return;
    }
    p++;

    size_t i = 0;
    while (*p && *p != '&' && i < out_len - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    url_decode(out);
}

static void wifi_http_server_stop(void) {
    if (s_http_server != NULL) {
        httpd_stop(s_http_server);
        s_http_server = NULL;
    }
}

static esp_err_t http_root_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, CONFIG_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_catch_all(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t wifi_connect_internal(const node_cfg_t *cfg, bool persist);
static void wifi_enable_config_ap(void);

static esp_err_t http_connect_post(httpd_req_t *req) {
    node_cfg_t cfg;
    cfg_defaults(&cfg);

    int len = req->content_len;
    if (len <= 0 || len > 1200) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }

    char body[1201];
    int r = httpd_req_recv(req, body, len);
    if (r <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Read failed");
        return ESP_FAIL;
    }
    body[r] = '\0';

    char tmp[32];
    parse_form_field(body, "wifi_ssid", cfg.wifi_ssid, sizeof(cfg.wifi_ssid));
    parse_form_field(body, "wifi_password", cfg.wifi_password, sizeof(cfg.wifi_password));
    parse_form_field(body, "mqtt_host", cfg.mqtt_host, sizeof(cfg.mqtt_host));
    parse_form_field(body, "ntp_host", cfg.ntp_host, sizeof(cfg.ntp_host));
    parse_form_field(body, "mqtt_username", cfg.mqtt_username, sizeof(cfg.mqtt_username));
    parse_form_field(body, "mqtt_password", cfg.mqtt_password, sizeof(cfg.mqtt_password));
    parse_form_field(body, "topic_base", cfg.topic_base, sizeof(cfg.topic_base));
    parse_form_field(body, "site_id", cfg.site_id, sizeof(cfg.site_id));
    parse_form_field(body, "node_id", cfg.node_id, sizeof(cfg.node_id));
    parse_form_field(body, "node_type", cfg.node_type, sizeof(cfg.node_type));
    parse_form_field(body, "mqtt_port", tmp, sizeof(tmp));
    if (tmp[0]) cfg.mqtt_port = atoi(tmp);
    parse_form_field(body, "pub_interval_ms", tmp, sizeof(tmp));
    if (tmp[0]) cfg.pub_interval_ms = atoi(tmp);

    if (cfg.wifi_ssid[0] == '\0' || cfg.mqtt_host[0] == '\0' || cfg.ntp_host[0] == '\0' || cfg.node_id[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing required fields");
        return ESP_FAIL;
    }
    if (cfg.mqtt_port <= 0) cfg.mqtt_port = 1883;
    if (cfg.pub_interval_ms < 500) cfg.pub_interval_ms = 2000;

    wifi_connect_internal(&cfg, true);

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req,
        "<html><body><h3>Config saved.</h3><p>Device is connecting to Wi-Fi and MQTT.</p></body></html>");
}

static esp_err_t http_status_get(httpd_req_t *req) {
    char json[384];
    snprintf(json, sizeof(json),
             "{\"wifi_connected\":%s,\"mqtt_connected\":%s,\"ap_mode\":%s,\"ssid\":\"%.32s\",\"mqtt_host\":\"%.63s\",\"ntp_host\":\"%.63s\",\"node_id\":\"%.32s\",\"status\":\"%.63s\"}",
             wifi_connected ? "true" : "false",
             mqtt_connected ? "true" : "false",
             wifi_ap_mode_enabled ? "true" : "false",
             g_sta_target_ssid,
             g_mqtt_host,
             g_ntp_host,
             g_node_id,
             g_status_line);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static void wifi_http_server_start(void) {
    if (s_http_server != NULL) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&s_http_server, &config) == ESP_OK) {
        httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = http_root_get};
        httpd_uri_t connect = {.uri = "/connect", .method = HTTP_POST, .handler = http_connect_post};
        httpd_uri_t status = {.uri = "/status", .method = HTTP_GET, .handler = http_status_get};
        httpd_uri_t catch_all = {.uri = "/*", .method = HTTP_GET, .handler = http_catch_all};
        httpd_register_uri_handler(s_http_server, &root);
        httpd_register_uri_handler(s_http_server, &connect);
        httpd_register_uri_handler(s_http_server, &status);
        httpd_register_uri_handler(s_http_server, &catch_all);
    }
}

static void wifi_disable_config_ap(void) {
    if (!wifi_ap_mode_enabled) return;

    captive_dns_stop();
    wifi_http_server_stop();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_ap_mode_enabled = false;
}

static void wifi_enable_config_ap(void) {
    wifi_config_t ap_cfg = {0};
    strncpy((char *)ap_cfg.ap.ssid, g_config_ap_ssid, sizeof(ap_cfg.ap.ssid) - 1);
    strncpy((char *)ap_cfg.ap.password, CONFIG_AP_PASS, sizeof(ap_cfg.ap.password) - 1);
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (!s_wifi_started) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());
        s_wifi_started = true;
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    }

    wifi_ap_mode_enabled = true;
    wifi_http_server_start();
    captive_dns_start();
    set_status("AP ready: %.20s", g_config_ap_ssid);
}

static esp_err_t wifi_connect_internal(const node_cfg_t *cfg, bool persist) {
    if (cfg == NULL || cfg->wifi_ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    node_cfg_t local = *cfg;
    if (local.mqtt_port <= 0) local.mqtt_port = 1883;
    if (local.pub_interval_ms < 500) local.pub_interval_ms = 2000;

    if (persist) {
        esp_err_t err = cfg_save(&local);
        if (err != ESP_OK) {
            set_status("NVS save failed");
            return err;
        }
    }

    cfg_apply_globals(&local);

    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid, local.wifi_ssid, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, local.wifi_password, sizeof(sta_cfg.sta.password) - 1);
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta_cfg.sta.pmf_cfg.capable = true;
    sta_cfg.sta.pmf_cfg.required = false;

    if (!s_wifi_started) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());
        s_wifi_started = true;
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_ap_mode_enabled ? WIFI_MODE_APSTA : WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    }

    s_wifi_retry_num = 0;
    wifi_connect_in_progress = true;
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_connect());
    set_status("Connecting WiFi: %.20s", local.wifi_ssid);
    return ESP_OK;
}

esp_err_t wifi_connect_credentials(const char *ssid, const char *password) {
    node_cfg_t cfg;
    cfg_defaults(&cfg);
    strncpy(cfg.wifi_ssid, ssid ? ssid : "", sizeof(cfg.wifi_ssid) - 1);
    strncpy(cfg.wifi_password, password ? password : "", sizeof(cfg.wifi_password) - 1);
    strncpy(cfg.mqtt_host, g_mqtt_host, sizeof(cfg.mqtt_host) - 1);
    strncpy(cfg.ntp_host, g_ntp_host, sizeof(cfg.ntp_host) - 1);
    cfg.mqtt_port = g_mqtt_port;
    strncpy(cfg.mqtt_username, g_mqtt_username, sizeof(cfg.mqtt_username) - 1);
    strncpy(cfg.mqtt_password, g_mqtt_password, sizeof(cfg.mqtt_password) - 1);
    strncpy(cfg.topic_base, g_topic_base, sizeof(cfg.topic_base) - 1);
    strncpy(cfg.site_id, g_site_id, sizeof(cfg.site_id) - 1);
    strncpy(cfg.node_id, g_node_id, sizeof(cfg.node_id) - 1);
    strncpy(cfg.node_type, g_node_type, sizeof(cfg.node_type) - 1);
    cfg.pub_interval_ms = g_pub_interval_ms;
    return wifi_connect_internal(&cfg, true);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (wifi_connect_in_progress) esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        mqtt_manager_stop();
        gpio_set_level(PIN_LED_GREEN, 0);
        gpio_set_level(PIN_LED_YELLOW, 1);

        if (wifi_connect_in_progress && s_wifi_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_wifi_retry_num++;
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
            set_status("WiFi retry %d/%d", s_wifi_retry_num, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            wifi_connect_in_progress = false;
            set_status("WiFi failed, AP enabled");
            wifi_enable_config_ap();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_retry_num = 0;
        wifi_connected = true;
        wifi_connect_in_progress = false;
        gpio_set_level(PIN_LED_GREEN, 1);
        gpio_set_level(PIN_LED_YELLOW, 0);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        sntp_start_from_rpi();
        set_status("WiFi connected");
        wifi_disable_config_ap();
        mqtt_manager_start();
    }
}

void wifi_init_sta(void) {
    strncpy(g_config_ap_ssid, "TGGS_SENSOR_NODE", sizeof(g_config_ap_ssid) - 1);
    g_config_ap_ssid[sizeof(g_config_ap_ssid) - 1] = '\0';

    if (wifi_event_group == NULL) {
        wifi_event_group = xEventGroupCreate();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    if (!s_wifi_initialized) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        s_wifi_initialized = true;
    }

    if (!s_handlers_registered) {
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_any_id_instance));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &wifi_got_ip_instance));
        s_handlers_registered = true;
    }

    node_cfg_t cfg;
    if (cfg_load(&cfg) == ESP_OK && cfg.wifi_ssid[0] != '\0') {
        wifi_connect_internal(&cfg, false);
    } else {
        cfg_defaults(&cfg);
        cfg_apply_globals(&cfg);
        wifi_enable_config_ap();
    }
}

const char *wifi_get_config_ap_ssid(void) {
    return g_config_ap_ssid;
}
