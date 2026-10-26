#include "led_control.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "board_pins.h"
#include "gpio_port.h"

typedef enum {
    LED_STATE_OFF,
    LED_STATE_ON,
} led_state_t;

static led_state_t led_state;
static uint32_t led_port_state;
static portMUX_TYPE led_state_lock = portMUX_INITIALIZER_UNLOCKED;

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

    return led_control_set(false);
}

/*
 * Establece el estado de la máquina del LED.
 * true = encendido, false = apagado.
 */
esp_err_t led_control_set(bool on)
{
    led_state_t next_state = on ? LED_STATE_ON : LED_STATE_OFF;

    portENTER_CRITICAL(&led_state_lock);
    esp_err_t err = gpio_port_write(&led_port_state, BOARD_LED_PORT_MASK,
                                    BOARD_LED_GPIO, next_state == LED_STATE_ON);
    if (err == ESP_OK) {
        led_state = next_state;
    }
    portEXIT_CRITICAL(&led_state_lock);

    return err;
}

/*
 * Invierte el estado actual del LED.
 * Útil para controles por botón o remoto IR.
 */
esp_err_t led_control_toggle(void)
{
    portENTER_CRITICAL(&led_state_lock);
    led_state_t next_state = led_state == LED_STATE_ON ? LED_STATE_OFF : LED_STATE_ON;
    esp_err_t err = gpio_port_write(&led_port_state, BOARD_LED_PORT_MASK,
                                    BOARD_LED_GPIO, next_state == LED_STATE_ON);
    if (err == ESP_OK) {
        led_state = next_state;
    }
    portEXIT_CRITICAL(&led_state_lock);
    return err;
}

/*
 * Devuelve el estado actual del LED sin modificar el hardware.
 */
bool led_control_is_on(void)
{
    portENTER_CRITICAL(&led_state_lock);
    bool on = led_state == LED_STATE_ON;
    portEXIT_CRITICAL(&led_state_lock);
    return on;
}