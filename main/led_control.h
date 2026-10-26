#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t led_control_init(void);
esp_err_t led_control_set(bool on);
esp_err_t led_control_toggle(void);
bool led_control_is_on(void);

#endif