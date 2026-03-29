#include <stdarg.h>
#include <stdio.h>

#include "esp_log.h"

#include "app_state.h"
#include "status.h"

#define TAG "IOT_DEMO"

void set_status(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_status_line, sizeof(g_status_line), fmt, ap);
    va_end(ap);
    ESP_LOGI(TAG, "%s", g_status_line);
}
