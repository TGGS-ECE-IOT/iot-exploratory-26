#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "esp_err.h"
#include "app_data.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t sensor_task_start(void);
bool sensor_data_copy(sensor_data_t *out);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_TASK_H
