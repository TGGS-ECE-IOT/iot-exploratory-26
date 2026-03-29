#include <stdio.h>
#include <time.h>

#include "esp_timer.h"

#include "utils.h"

uint64_t now_ms(void) {
    return esp_timer_get_time() / 1000ULL;
}

void get_iso_time(char *out, size_t out_len) {
    time_t now;
    struct tm info;
    time(&now);
    if (now > 1700000000) {
        localtime_r(&now, &info);
        strftime(out, out_len, "%Y-%m-%d %H:%M:%S", &info);
    } else {
        snprintf(out, out_len, "uptime:%llu", (unsigned long long)now_ms());
    }
}

static float clampf(float x, float minv, float maxv) {
    if (x < minv) return minv;
    if (x > maxv) return maxv;
    return x;
}

float raw_to_pct(int raw) {
    return clampf((raw / 4095.0f) * 100.0f, 0.0f, 100.0f);
}
