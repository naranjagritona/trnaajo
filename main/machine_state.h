#ifndef MACHINE_STATE_H
#define MACHINE_STATE_H

#include <stdint.h>

#include "esp_err.h"

#define ALARM_OFF_BIT 0b00000000U
#define ALARM_ON_BIT 0b00000001U
#define ALARM_TRIGGERED_BIT 0b00000010U
#define LED_BIT 0b00000011U
#define MOTOR_BIT 0b00000100U
#define BUTTON_BIT 0b00000101U

#define MACHINE_STATE_ALARM_OFF ((uint8_t)0b00000001U)
#define MACHINE_STATE_ALARM_ON ((uint8_t)0b00000010U)
#define MACHINE_STATE_ALARM_TRIGGERED ((uint8_t)0b00000100U)
#define MACHINE_STATE_ALARM_MASK ((uint8_t)0b00000111U)
#define MACHINE_STATE_LED_MASK ((uint8_t)0b00001000U)
#define MACHINE_STATE_MOTOR_MASK ((uint8_t)0b00010000U)
#define MACHINE_STATE_BUTTON_MASK ((uint8_t)0b00100000U)
#define MACHINE_STATE_OUTPUT_MASK ((uint8_t)0b00011111U)

esp_err_t machine_state_init(void);
esp_err_t machine_state_configure_output(uint8_t mask);
esp_err_t machine_state_configure_input(uint8_t mask);
esp_err_t machine_state_set_output(uint8_t mask, uint8_t value);
uint8_t machine_state_get_output(uint8_t mask);
esp_err_t machine_state_set_input(uint8_t mask, uint8_t value);
uint8_t machine_state_get_input(uint8_t mask);

#endif
