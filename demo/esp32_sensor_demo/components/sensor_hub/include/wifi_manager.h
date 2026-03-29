#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"

void wifi_init_sta(void);
esp_err_t wifi_connect_credentials(const char *ssid, const char *password);
const char *wifi_get_config_ap_ssid(void);

#endif
