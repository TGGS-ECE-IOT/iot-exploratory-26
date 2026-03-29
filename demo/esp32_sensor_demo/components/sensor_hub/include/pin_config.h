#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include "driver/gpio.h"

/* UI */
#define PIN_BUZZER          GPIO_NUM_23

#define PIN_LED_RED         GPIO_NUM_25
#define PIN_LED_YELLOW      GPIO_NUM_26
#define PIN_LED_GREEN       GPIO_NUM_27

#define PIN_BTN_RED         GPIO_NUM_14
#define PIN_BTN_BLUE        GPIO_NUM_12

#define PIN_OLED_SDA        GPIO_NUM_21
#define PIN_OLED_SCL        GPIO_NUM_22

/* Sensors */
#define PIN_DHT22           GPIO_NUM_13

#define PIN_HCSR04_TRIG     GPIO_NUM_32
#define PIN_HCSR04_ECHO     GPIO_NUM_33

#define PIN_PIR             GPIO_NUM_35

#define PIN_MQ135_ADC       6 /* ADC1 channel 6 / GPIO34 */
#define PIN_LDR_ADC         3 /* ADC1 channel 3 / GPIO39 (VN) */
#define PIN_POT_ADC         0 /* ADC1 channel 0 / GPIO36 (VP) */

#endif
