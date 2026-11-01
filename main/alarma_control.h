#ifndef ALARMA_CONTROL_H
#define ALARMA_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "machine_state.h"

typedef uint8_t machine_state_t;

#define MACHINE_STATE_OFF MACHINE_STATE_ALARM_OFF
#define MACHINE_STATE_ON MACHINE_STATE_ALARM_ON
#define MACHINE_STATE_TRIGGERED MACHINE_STATE_ALARM_TRIGGERED

esp_err_t alarma_control_init(void);
void alarma_control_toggle(void);
esp_err_t alarma_control_set_enabled(bool enabled);
machine_state_t alarma_control_get_state(void);
esp_err_t alarma_control_trigger(void);

#endif
