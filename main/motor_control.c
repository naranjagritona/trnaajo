#include "motor_control.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "board_pins.h"
#include "gpio_port.h"

typedef enum {
    MOTOR_STATE_OFF,
    MOTOR_STATE_ON,
} motor_state_t;

static motor_state_t motor_state;
static uint32_t motor_port_state;
static portMUX_TYPE motor_state_lock = portMUX_INITIALIZER_UNLOCKED;

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

    return motor_control_set(false);
}

/*
 * Activa o desactiva el motor según el valor recibido.
 */
esp_err_t motor_control_set(bool on)
{
    motor_state_t next_state = on ? MOTOR_STATE_ON : MOTOR_STATE_OFF;

    portENTER_CRITICAL(&motor_state_lock);
    esp_err_t err = gpio_port_write(&motor_port_state, BOARD_MOTOR_PORT_MASK,
                                    BOARD_MOTOR_GPIO, next_state == MOTOR_STATE_ON);
    if (err == ESP_OK) {
        motor_state = next_state;
    }
    portEXIT_CRITICAL(&motor_state_lock);

    return err;
}

/*
 * Cambia el estado del motor de forma invertida al estado actual.
 */
esp_err_t motor_control_toggle(void)
{
    portENTER_CRITICAL(&motor_state_lock);
    motor_state_t next_state = motor_state == MOTOR_STATE_ON ? MOTOR_STATE_OFF : MOTOR_STATE_ON;
    esp_err_t err = gpio_port_write(&motor_port_state, BOARD_MOTOR_PORT_MASK,
                                    BOARD_MOTOR_GPIO, next_state == MOTOR_STATE_ON);
    if (err == ESP_OK) {
        motor_state = next_state;
    }
    portEXIT_CRITICAL(&motor_state_lock);
    return err;
}

/*
 * Consulta el estado actual del motor sin modificar el hardware.
 */
bool motor_control_is_on(void)
{
    portENTER_CRITICAL(&motor_state_lock);
    bool on = motor_state == MOTOR_STATE_ON;
    portEXIT_CRITICAL(&motor_state_lock);
    return on;
}