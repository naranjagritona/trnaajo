#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t motor_control_init(void);
esp_err_t motor_control_set(bool on);
esp_err_t motor_control_toggle(void);
bool motor_control_is_on(void);

#endif