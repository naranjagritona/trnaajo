#include "led_control.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#define LED_GPIO GPIO_NUM_22

static bool led_is_on;
static portMUX_TYPE led_state_lock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t led_control_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << LED_GPIO,
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

esp_err_t led_control_set(bool on)
{
    portENTER_CRITICAL(&led_state_lock);
    esp_err_t err = gpio_set_level(LED_GPIO, on ? 1 : 0);
    if (err == ESP_OK) {
        led_is_on = on;
    }
    portEXIT_CRITICAL(&led_state_lock);

    return err;
}

esp_err_t led_control_toggle(void)
{
    portENTER_CRITICAL(&led_state_lock);
    bool next = !led_is_on;
    esp_err_t err = gpio_set_level(LED_GPIO, next ? 1 : 0);
    if (err == ESP_OK) {
        led_is_on = next;
    }
    portEXIT_CRITICAL(&led_state_lock);
    return err;
}

bool led_control_is_on(void)
{
    portENTER_CRITICAL(&led_state_lock);
    bool on = led_is_on;
    portEXIT_CRITICAL(&led_state_lock);
    return on;
}