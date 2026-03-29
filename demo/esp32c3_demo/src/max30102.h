#ifndef MAX30102_H
#define MAX30102_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t port;
    uint8_t addr;

    float ir_dc;
    uint32_t last_beat_ms;
    int bpm;
    bool last_above;
} max30102_t;

typedef struct {
    uint32_t red;
    uint32_t ir;
    int bpm;
    bool finger_present;
} max30102_data_t;

esp_err_t max30102_init(max30102_t *dev, i2c_port_t port, uint8_t addr);
esp_err_t max30102_read(max30102_t *dev, max30102_data_t *out, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif // MAX30102_H
