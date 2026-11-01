#include "motor_control.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "board_pins.h"
#include "machine_state.h"

static portMUX_TYPE motor_control_lock = portMUX_INITIALIZER_UNLOCKED;

/*
 * Inicializa todos los GPIO usados para el motor y sus indicadores LED.
 * Se configura como salida y se deja todo apagado al inicio.
 */
esp_err_t motor_control_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOARD_MOTOR_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }

    err = machine_state_configure_output(MACHINE_STATE_MOTOR_MASK);
    if (err != ESP_OK) {
        return err;
    }

    return motor_control_set(false);
}

/*
 * Activa o desactiva el motor según el valor recibido.
 */
esp_err_t motor_control_set(bool on)
{
    portENTER_CRITICAL(&motor_control_lock);
    esp_err_t err = gpio_set_level(BOARD_MOTOR_GPIO, on);
    if (err == ESP_OK) {
        err = machine_state_set_output(
            MACHINE_STATE_MOTOR_MASK, on ? MACHINE_STATE_MOTOR_MASK : 0b00000000U);
    }
    portEXIT_CRITICAL(&motor_control_lock);
    return err;
}

/*
 * Cambia el estado del motor de forma invertida al estado actual.
 */
esp_err_t motor_control_toggle(void)
{
    portENTER_CRITICAL(&motor_control_lock);
    bool on = machine_state_get_output(MACHINE_STATE_MOTOR_MASK) == 0b00000000U;
    esp_err_t err = gpio_set_level(BOARD_MOTOR_GPIO, on);
    if (err == ESP_OK) {
        err = machine_state_set_output(
            MACHINE_STATE_MOTOR_MASK, on ? MACHINE_STATE_MOTOR_MASK : 0b00000000U);
    }
    portEXIT_CRITICAL(&motor_control_lock);
    return err;
}

/*
 * Consulta el estado actual del motor sin modificar el hardware.
 */
bool motor_control_is_on(void)
{
    return machine_state_get_output(MACHINE_STATE_MOTOR_MASK) != 0b00000000U;
}