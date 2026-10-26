#include "motor_control.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#define MOTOR_GPIO GPIO_NUM_16
#define LED_YELLOW_GPIO GPIO_NUM_27
#define LED_RED_GPIO GPIO_NUM_26
#define LED_GREEN_GPIO GPIO_NUM_25

static bool motor_is_on;
static portMUX_TYPE motor_state_lock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t motor_control_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << MOTOR_GPIO) |
                (1ULL << LED_YELLOW_GPIO) |
                (1ULL << LED_RED_GPIO) |
                (1ULL << LED_GREEN_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }

    gpio_set_level(LED_YELLOW_GPIO, 0);
    gpio_set_level(LED_RED_GPIO, 0);
    gpio_set_level(LED_GREEN_GPIO, 0);
    return motor_control_set(false);
}

esp_err_t motor_control_set(bool on)
{
    portENTER_CRITICAL(&motor_state_lock);
    esp_err_t err = gpio_set_level(MOTOR_GPIO, on ? 1 : 0);
    if (err == ESP_OK) {
        motor_is_on = on;
    }
    portEXIT_CRITICAL(&motor_state_lock);

    return err;
}

esp_err_t motor_control_toggle(void)
{
    portENTER_CRITICAL(&motor_state_lock);
    bool next = !motor_is_on;
    esp_err_t err = gpio_set_level(MOTOR_GPIO, next ? 1 : 0);
    if (err == ESP_OK) {
        motor_is_on = next;
    }
    portEXIT_CRITICAL(&motor_state_lock);
    return err;
}

bool motor_control_is_on(void)
{
    portENTER_CRITICAL(&motor_state_lock);
    bool on = motor_is_on;
    portEXIT_CRITICAL(&motor_state_lock);
    return on;
}