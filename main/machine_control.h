#ifndef MACHINE_CONTROL_H
#define MACHINE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef uint8_t machine_state_t;

#define MACHINE_STATE_OFF ((machine_state_t)0b00000001U)
#define MACHINE_STATE_ON ((machine_state_t)0b00000010U)
#define MACHINE_STATE_TRIGGERED ((machine_state_t)0b00000100U)

esp_err_t machine_control_init(void);
void machine_control_toggle(void);
esp_err_t machine_control_set_enabled(bool enabled);
machine_state_t machine_control_get_state(void);
esp_err_t machine_control_trigger(void);

#endif
