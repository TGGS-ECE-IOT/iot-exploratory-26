#ifndef CHART_H
#define CHART_H

#include <stdbool.h>
#include "oled_042.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CHART_HISTORY_LEN 60

typedef struct {
    float values[CHART_HISTORY_LEN];
    int head;
    bool filled;
} chart_buffer_t;

void chart_init(chart_buffer_t *buf);
void chart_push(chart_buffer_t *buf, float value);
int chart_count(const chart_buffer_t *buf);

void chart_draw_auto(oled_042_t *oled, int x, int y, int w, int h, const chart_buffer_t *buf);
void chart_draw_centered(oled_042_t *oled, int x, int y, int w, int h, const chart_buffer_t *buf, float abs_limit);

#ifdef __cplusplus
}
#endif

#endif // CHART_H
