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
#include "machine_state.h"

#define BUTTON_DEBOUNCE_MS 0b00011001U
#define BUTTON_TASK_STACK_SIZE 0b110000000000U

static const char *TAG = "MACHINE";
static const struct {
    gpio_num_t gpio;
    uint8_t bit;
} state_outputs[] = {
    {BOARD_STATE_OFF_GPIO, ALARM_OFF_BIT},
    {BOARD_STATE_ON_GPIO, ALARM_ON_BIT},
    {BOARD_STATE_TRIGGERED_GPIO, ALARM_TRIGGERED_BIT},
};

static portMUX_TYPE machine_state_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t button_task_handle;

static esp_err_t machine_control_write_outputs(machine_state_t state)
{
    if (state != MACHINE_STATE_OFF &&
        state != MACHINE_STATE_ON &&
        state != MACHINE_STATE_TRIGGERED) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;
    for (size_t i = 0b00000000U;
         i < sizeof(state_outputs) / sizeof(state_outputs[0b00000000]); i++) {
        esp_err_t err = gpio_set_level(
            state_outputs[i].gpio,
            (state & (uint8_t)(0b00000001U << state_outputs[i].bit)) != 0b00000000U);
        if (result == ESP_OK && err != ESP_OK) {
            result = err;
        }
    }
    if (result == ESP_OK) {
        result = machine_state_set_output(MACHINE_STATE_ALARM_MASK, state);
    }
    return result;
}

static uint8_t machine_control_read_button(void)
{
    uint8_t button_level = gpio_get_level(BOARD_STATE_BUTTON_GPIO) != 0b00000000
                               ? MACHINE_STATE_BUTTON_MASK
                               : 0b00000000U;
    esp_err_t err = machine_state_set_input(MACHINE_STATE_BUTTON_MASK, button_level);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo actualizar la entrada del pulsador: %s",
                 esp_err_to_name(err));
        return 0b00000000U;
    }
    return machine_state_get_input(MACHINE_STATE_BUTTON_MASK) != 0b00000000U
               ? 0b00000001U
               : 0b00000000U;
}

static void wait_for_button_release(void)
{
    for (;;) {
        while (machine_control_read_button() == 0b00000000U) {
            vTaskDelay(pdMS_TO_TICKS(0b00001010U));
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
        if (machine_control_read_button() != 0b00000000U) {
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

        if (machine_control_read_button() == 0b00000000U) {
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
    const uint64_t output_pin_mask = (0b00000001ULL << BOARD_STATE_OFF_GPIO) |
                                    (0b00000001ULL << BOARD_STATE_ON_GPIO) |
                                    (0b00000001ULL << BOARD_STATE_TRIGGERED_GPIO);

    const gpio_config_t output_config = {
        .pin_bit_mask = output_pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&output_config);
    if (err != ESP_OK) {
        return err;
    }

    err = machine_state_configure_output(MACHINE_STATE_ALARM_MASK);
    if (err != ESP_OK) {
        return err;
    }

    for (size_t i = 0b00000000U;
         i < sizeof(state_outputs) / sizeof(state_outputs[0b00000000]); i++) {
        err = gpio_set_level(state_outputs[i].gpio, 0b00000000);
        if (err != ESP_OK) {
            return err;
        }
    }

    portENTER_CRITICAL(&machine_state_lock);
    err = machine_control_write_outputs(MACHINE_STATE_OFF);
    portEXIT_CRITICAL(&machine_state_lock);
    if (err != ESP_OK) {
        return err;
    }

    const gpio_config_t button_config = {
        .pin_bit_mask = 0b00000001ULL << BOARD_STATE_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    err = gpio_config(&button_config);
    if (err != ESP_OK) {
        return err;
    }

    err = machine_state_configure_input(MACHINE_STATE_BUTTON_MASK);
    if (err != ESP_OK) {
        return err;
    }

    if (xTaskCreate(button_task, "machine_button", BUTTON_TASK_STACK_SIZE,
                    NULL, 0b00000110U, &button_task_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    err = gpio_install_isr_service(0b00000000);
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
    machine_state_t current_state =
        (machine_state_t)machine_state_get_output(MACHINE_STATE_ALARM_MASK);
    machine_state_t next_state = current_state == MACHINE_STATE_OFF
                                     ? MACHINE_STATE_ON
                                     : MACHINE_STATE_OFF;
    esp_err_t err = machine_control_write_outputs(next_state);
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
    portEXIT_CRITICAL(&machine_state_lock);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Máquina %s", enabled ? "PRENDIDA" : "APAGADA");
    } else {
        ESP_LOGE(TAG, "No se pudo actualizar los indicadores: %s", esp_err_to_name(err));
    }
    return err;
}

machine_state_t machine_control_get_state(void)
{
    portENTER_CRITICAL(&machine_state_lock);
    machine_state_t state =
        (machine_state_t)machine_state_get_output(MACHINE_STATE_ALARM_MASK);
    portEXIT_CRITICAL(&machine_state_lock);
    return state;
}

esp_err_t machine_control_trigger(void)
{
    portENTER_CRITICAL(&machine_state_lock);
    if (machine_state_get_output(MACHINE_STATE_ALARM_MASK) != MACHINE_STATE_ON) {
        portEXIT_CRITICAL(&machine_state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = machine_control_write_outputs(MACHINE_STATE_TRIGGERED);
    portEXIT_CRITICAL(&machine_state_lock);
    return err;
}
