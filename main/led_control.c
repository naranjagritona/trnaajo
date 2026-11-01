#include "led_control.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "board_pins.h"
#include "machine_state.h"

static portMUX_TYPE led_control_lock = portMUX_INITIALIZER_UNLOCKED;

/*
 * Configura el GPIO del LED como salida digital.
 * Luego deja el LED apagado al arrancar.
 */
esp_err_t led_control_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOARD_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }

    err = machine_state_configure_output(MACHINE_STATE_LED_MASK);
    if (err != ESP_OK) {
        return err;
    }

    return led_control_set(false);
}

/*
 * Establece el estado de la máquina del LED.
 * true = encendido, false = apagado.
 */
esp_err_t led_control_set(bool on)
{
    portENTER_CRITICAL(&led_control_lock);
    esp_err_t err = gpio_set_level(BOARD_LED_GPIO, on);
    if (err == ESP_OK) {
        err = machine_state_set_output(
            MACHINE_STATE_LED_MASK, on ? MACHINE_STATE_LED_MASK : 0b00000000U);
    }
    portEXIT_CRITICAL(&led_control_lock);
    return err;
}

/*
 * Invierte el estado actual del LED.
 * Útil para controles por botón o remoto IR.
 */
esp_err_t led_control_toggle(void)
{
    portENTER_CRITICAL(&led_control_lock);
    bool on = machine_state_get_output(MACHINE_STATE_LED_MASK) == 0b00000000U;
    esp_err_t err = gpio_set_level(BOARD_LED_GPIO, on);
    if (err == ESP_OK) {
        err = machine_state_set_output(
            MACHINE_STATE_LED_MASK, on ? MACHINE_STATE_LED_MASK : 0b00000000U);
    }
    portEXIT_CRITICAL(&led_control_lock);
    return err;
}

/*
 * Devuelve el estado actual del LED sin modificar el hardware.
 */
bool led_control_is_on(void)
{
    return machine_state_get_output(MACHINE_STATE_LED_MASK) != 0b00000000U;
}