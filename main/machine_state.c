#include "machine_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static uint8_t DDRC = 0b00000000;
static uint8_t PORTC = 0b00000000;
static uint8_t PINC = 0b00000000;
static portMUX_TYPE machine_state_lock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t machine_state_init(void)
{
    portENTER_CRITICAL(&machine_state_lock);
    DDRC = 0b00000000;
    PORTC = 0b00000000;
    PINC = 0b00000000;
    portEXIT_CRITICAL(&machine_state_lock);
    return ESP_OK;
}

esp_err_t machine_state_configure_output(uint8_t mask)
{
    if (mask == 0b00000000U) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&machine_state_lock);
    DDRC |= mask;
    portEXIT_CRITICAL(&machine_state_lock);
    return ESP_OK;
}

esp_err_t machine_state_configure_input(uint8_t mask)
{
    if (mask == 0b00000000U) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&machine_state_lock);
    DDRC &= (uint8_t)~mask;
    portEXIT_CRITICAL(&machine_state_lock);
    return ESP_OK;
}

esp_err_t machine_state_set_output(uint8_t mask, uint8_t value)
{
    if (mask == 0b00000000U || (value & (uint8_t)~mask) != 0b00000000U) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&machine_state_lock);
    if ((DDRC & mask) != mask) {
        portEXIT_CRITICAL(&machine_state_lock);
        return ESP_ERR_INVALID_STATE;
    }

    PORTC = (uint8_t)((PORTC & (uint8_t)~mask) | value);
    portEXIT_CRITICAL(&machine_state_lock);
    return ESP_OK;
}

uint8_t machine_state_get_output(uint8_t mask)
{
    portENTER_CRITICAL(&machine_state_lock);
    uint8_t value = (uint8_t)(PORTC & mask);
    portEXIT_CRITICAL(&machine_state_lock);
    return value;
}

esp_err_t machine_state_set_input(uint8_t mask, uint8_t value)
{
    if (mask == 0b00000000U || (value & (uint8_t)~mask) != 0b00000000U) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&machine_state_lock);
    if ((DDRC & mask) != 0b00000000U) {
        portEXIT_CRITICAL(&machine_state_lock);
        return ESP_ERR_INVALID_STATE;
    }

    PINC = (uint8_t)((PINC & (uint8_t)~mask) | value);
    portEXIT_CRITICAL(&machine_state_lock);
    return ESP_OK;
}

uint8_t machine_state_get_input(uint8_t mask)
{
    portENTER_CRITICAL(&machine_state_lock);
    uint8_t value = (uint8_t)(PINC & mask);
    portEXIT_CRITICAL(&machine_state_lock);
    return value;
}
