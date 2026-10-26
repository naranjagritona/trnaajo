#include "machine_control.h"

#include <stdbool.h>
#include <stddef.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

#include "board_pins.h"

#define BUTTON_DEBOUNCE_MS 25
#define BUTTON_TASK_STACK_SIZE 3072

static const char *TAG = "MACHINE";
static const gpio_num_t state_gpios[] = {
    BOARD_STATE_OFF_GPIO,
    BOARD_STATE_ON_GPIO,
    BOARD_STATE_TRIGGERED_GPIO,
};

static machine_state_t machine_state;
static portMUX_TYPE machine_state_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t button_task_handle;

static esp_err_t machine_control_write_outputs(machine_state_t state)
{
    esp_err_t result = ESP_OK;
    for (size_t i = 0; i < sizeof(state_gpios) / sizeof(state_gpios[0]); i++) {
        esp_err_t err = gpio_set_level(state_gpios[i], i == (size_t)state);
        if (result == ESP_OK && err != ESP_OK) {
            result = err;
        }
    }
    return result;
}

static void wait_for_button_release(void)
{
    for (;;) {
        while (gpio_get_level(BOARD_STATE_BUTTON_GPIO) == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
        if (gpio_get_level(BOARD_STATE_BUTTON_GPIO) != 0) {
            return;
        }
    }
}

static void button_task(void *argument)
{
    (void)argument;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));

        if (gpio_get_level(BOARD_STATE_BUTTON_GPIO) == 0) {
            esp_err_t err = machine_control_trigger();
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Pulsador: estado DISPARADA");
            } else if (err != ESP_ERR_INVALID_STATE) {
                ESP_LOGE(TAG, "No se pudo disparar la máquina: %s", esp_err_to_name(err));
            }
            wait_for_button_release();
        }
    }
}

static void button_isr_handler(void *argument)
{
    (void)argument;
    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(button_task_handle, &higher_priority_task_woken);
    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t machine_control_init(void)
{
    const gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << BOARD_STATE_TRIGGERED_GPIO) |
                        (1ULL << BOARD_STATE_OFF_GPIO) |
                        (1ULL << BOARD_STATE_ON_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&output_config);
    if (err != ESP_OK) {
        return err;
    }

    for (size_t i = 0; i < sizeof(state_gpios) / sizeof(state_gpios[0]); i++) {
        err = gpio_set_level(state_gpios[i], 0);
        if (err != ESP_OK) {
            return err;
        }
    }

    machine_state = MACHINE_STATE_OFF;
    portENTER_CRITICAL(&machine_state_lock);
    err = machine_control_write_outputs(machine_state);
    portEXIT_CRITICAL(&machine_state_lock);
    if (err != ESP_OK) {
        return err;
    }

    const gpio_config_t button_config = {
        .pin_bit_mask = 1ULL << BOARD_STATE_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    err = gpio_config(&button_config);
    if (err != ESP_OK) {
        return err;
    }

    if (xTaskCreate(button_task, "machine_button", BUTTON_TASK_STACK_SIZE,
                    NULL, 6, &button_task_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        vTaskDelete(button_task_handle);
        button_task_handle = NULL;
        return err;
    }

    err = gpio_isr_handler_add(BOARD_STATE_BUTTON_GPIO, button_isr_handler, NULL);
    if (err != ESP_OK) {
        vTaskDelete(button_task_handle);
        button_task_handle = NULL;
        return err;
    }

    ESP_LOGI(TAG, "Máquina lista: GPIO36 pulsador, indicadores GPIO25/26/27");
    return ESP_OK;
}

void machine_control_toggle(void)
{
    portENTER_CRITICAL(&machine_state_lock);
    machine_state_t next_state = machine_state == MACHINE_STATE_OFF
                                     ? MACHINE_STATE_ON
                                     : MACHINE_STATE_OFF;
    esp_err_t err = machine_control_write_outputs(next_state);
    if (err == ESP_OK) {
        machine_state = next_state;
    }
    portEXIT_CRITICAL(&machine_state_lock);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Máquina %s", next_state == MACHINE_STATE_ON ? "PRENDIDA" : "APAGADA");
    } else {
        ESP_LOGE(TAG, "No se pudo actualizar los indicadores: %s", esp_err_to_name(err));
    }
}

esp_err_t machine_control_set_enabled(bool enabled)
{
    machine_state_t next_state = enabled ? MACHINE_STATE_ON : MACHINE_STATE_OFF;

    portENTER_CRITICAL(&machine_state_lock);
    esp_err_t err = machine_control_write_outputs(next_state);
    if (err == ESP_OK) {
        machine_state = next_state;
    }
    portEXIT_CRITICAL(&machine_state_lock);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Máquina %s", enabled ? "PRENDIDA" : "APAGADA");
    } else {
        ESP_LOGE(TAG, "No se pudo actualizar los indicadores: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t machine_control_trigger(void)
{
    portENTER_CRITICAL(&machine_state_lock);
    if (machine_state != MACHINE_STATE_ON) {
        portEXIT_CRITICAL(&machine_state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = machine_control_write_outputs(MACHINE_STATE_TRIGGERED);
    if (err == ESP_OK) {
        machine_state = MACHINE_STATE_TRIGGERED;
    }
    portEXIT_CRITICAL(&machine_state_lock);
    return err;
}
