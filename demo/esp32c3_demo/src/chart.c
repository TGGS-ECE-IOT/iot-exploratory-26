#include "chart.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int clamp_i(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void draw_line_safe(oled_042_t *oled, int x0, int y0, int x1, int y1, bool color)
{
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        oled_042_set_pixel(oled, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }

        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_frame(oled_042_t *oled, int x, int y, int w, int h)
{
    int mid = y + h / 2;
    for (int xx = x + 1; xx < x + w - 1; xx += 2) {
        oled_042_set_pixel(oled, xx, mid, true);
    }
}

static void draw_chart(oled_042_t *oled, int x, int y, int w, int h,
                       const chart_buffer_t *buf, float min_v, float max_v)
{
    draw_frame(oled, x, y, w, h);

    int count = chart_count(buf);
    if (count < 2) {
        return;
    }

    float range = max_v - min_v;
    if (fabsf(range) < 0.0001f) {
        range = 1.0f;
        min_v -= 0.5f;
        max_v += 0.5f;
    }

    int start = buf->filled ? buf->head : 0;

    int prev_x = 0;
    int prev_y = 0;
    bool has_prev = false;

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % CHART_HISTORY_LEN;
        float v = buf->values[idx];

        int px = x + 1 + (i * (w - 3)) / (count - 1);
        float norm = (v - min_v) / range;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;

        int py = y + h - 2 - (int)lrintf(norm * (float)(h - 3));
        py = clamp_i(py, y + 1, y + h - 2);

        if (has_prev) {
            draw_line_safe(oled, prev_x, prev_y, px, py, true);
        }

        prev_x = px;
        prev_y = py;
        has_prev = true;
    }
}

void chart_init(chart_buffer_t *buf)
{
    if (!buf) {
        return;
    }
    memset(buf, 0, sizeof(*buf));
}

void chart_push(chart_buffer_t *buf, float value)
{
    if (!buf) {
        return;
    }

    buf->values[buf->head] = value;
    buf->head = (buf->head + 1) % CHART_HISTORY_LEN;
    if (buf->head == 0) {
        buf->filled = true;
    }
}

int chart_count(const chart_buffer_t *buf)
{
    if (!buf) {
        return 0;
    }
    return buf->filled ? CHART_HISTORY_LEN : buf->head;
}

void chart_draw_auto(oled_042_t *oled, int x, int y, int w, int h, const chart_buffer_t *buf)
{
    int count = chart_count(buf);
    if (count <= 0) {
        draw_frame(oled, x, y, w, h);
        return;
    }

    int start = buf->filled ? buf->head : 0;
    float min_v = 0.0f;
    float max_v = 0.0f;
    bool first = true;

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % CHART_HISTORY_LEN;
        float v = buf->values[idx];

        if (first) {
            min_v = max_v = v;
            first = false;
        } else {
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
        }
    }

    if (fabsf(max_v - min_v) < 0.001f) {
        min_v -= 1.0f;
        max_v += 1.0f;
    }

    draw_chart(oled, x, y, w, h, buf, min_v, max_v);
}

void chart_draw_centered(oled_042_t *oled, int x, int y, int w, int h, const chart_buffer_t *buf, float abs_limit)
{
    if (abs_limit < 0.001f) {
        abs_limit = 1.0f;
    }
    draw_chart(oled, x, y, w, h, buf, -abs_limit, abs_limit);
}
