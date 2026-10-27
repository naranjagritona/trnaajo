#ifndef MACHINE_CONTROL_H
#define MACHINE_CONTROL_H

#include <stdbool.h>

#include "esp_err.h"

typedef enum {
    MACHINE_STATE_OFF,
    MACHINE_STATE_ON,
    MACHINE_STATE_TRIGGERED,
} machine_state_t;

esp_err_t machine_control_init(void);
void machine_control_toggle(void);
esp_err_t machine_control_set_enabled(bool enabled);
machine_state_t machine_control_get_state(void);
esp_err_t machine_control_trigger(void);

#endif
