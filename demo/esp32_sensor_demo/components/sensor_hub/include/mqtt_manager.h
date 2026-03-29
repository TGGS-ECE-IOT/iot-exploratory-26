#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

void mqtt_manager_start(void);
void mqtt_manager_stop(void);
void mqtt_tx_task(void *arg);

#endif
