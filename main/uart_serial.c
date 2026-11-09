#include "uart_serial.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "alarma_control.h"
#include "board_pins.h"
#include "dht11_sensor.h"
#include "led_control.h"
#include "machine_state.h"
#include "motor_control.h"

#define SERIAL_UART_NUM UART_NUM_2
#define SERIAL_RX_BUFFER_SIZE 512
#define SERIAL_LINE_SIZE 64
#define SERIAL_TASK_STACK_SIZE 3072
#define SERIAL_BAUD_RATE 9600

static const char *TAG = "UART";

static esp_err_t send_line(const char *line)
{
    size_t length = strlen(line);
    int sent = uart_write_bytes(SERIAL_UART_NUM, line, length);
    return sent == (int)length ? ESP_OK : ESP_FAIL;
}

static void send_error(const char *message)
{
    char response[96];
    int length = snprintf(response, sizeof(response), "ERR %s\r\n", message);
    if (length > 0 && (size_t)length < sizeof(response)) {
        (void)send_line(response);
    }
}

static esp_err_t send_status(void)
{
    machine_state_t alarm_state = alarma_control_get_state();
    const char *alarm_name;
    switch (alarm_state) {
    case MACHINE_STATE_OFF:
        alarm_name = "apagada";
        break;
    case MACHINE_STATE_ON:
        alarm_name = "prendida";
        break;
    case MACHINE_STATE_TRIGGERED:
        alarm_name = "disparada";
        break;
    default:
        send_error("estado de alarma invalido");
        return ESP_ERR_INVALID_STATE;
    }

    int16_t temperature_tenths = 0;
    int16_t humidity_tenths = 0;
    bool sensor_available = dht11_sensor_get_reading(
        &temperature_tenths, &humidity_tenths);

    char response[192];
    int length = snprintf(
        response, sizeof(response),
        "{\"state\":\"%s\",\"state_code\":%u,\"led\":\"%s\",\"motor\":\"%s\","
        "\"sensor_available\":%s,\"temperature_tenths\":%d,\"humidity_tenths\":%d}\r\n",
        alarm_name, (unsigned int)alarm_state,
        led_control_is_on() ? "prendido" : "apagado",
        motor_control_is_on() ? "prendido" : "apagado",
        sensor_available ? "true" : "false",
        (int)temperature_tenths, (int)humidity_tenths);
    if (length < 0 || (size_t)length >= sizeof(response)) {
        send_error("no se pudo generar el estado");
        return ESP_FAIL;
    }
    return send_line(response);
}

static void process_command(const char *command)
{
    esp_err_t err;
    if (strcmp(command, "STATUS") == 0) {
        (void)send_status();
        return;
    } else if (strcmp(command, "LED ON") == 0) {
        err = led_control_set(true);
    } else if (strcmp(command, "LED OFF") == 0) {
        err = led_control_set(false);
    } else if (strcmp(command, "MOTOR ON") == 0) {
        err = motor_control_set(true);
    } else if (strcmp(command, "MOTOR OFF") == 0) {
        err = motor_control_set(false);
    } else if (strcmp(command, "ALARM ON") == 0) {
        err = alarma_control_set_enabled(true);
    } else if (strcmp(command, "ALARM OFF") == 0) {
        err = alarma_control_set_enabled(false);
    } else {
        send_error("comando no reconocido");
        return;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falló '%s': %s", command, esp_err_to_name(err));
        send_error("no se pudo aplicar el comando");
        return;
    }
    (void)send_line("OK\r\n");
}

static void uart_command_task(void *argument)
{
    (void)argument;
    uint8_t input[128];
    char line[SERIAL_LINE_SIZE];
    size_t line_length = 0;
    bool discard_line = false;
    TickType_t last_status_tick = xTaskGetTickCount();

    for (;;) {
        int received = uart_read_bytes(
            SERIAL_UART_NUM, input, sizeof(input), pdMS_TO_TICKS(100));
        for (int i = 0; i < received; ++i) {
            uint8_t value = input[i];
            if (value == '\r' || value == '\n') {
                if (discard_line) {
                    send_error("comando demasiado largo");
                } else if (line_length > 0) {
                    line[line_length] = '\0';
                    ESP_LOGI(TAG, "Comando UART: %s", line);
                    process_command(line);
                }
                line_length = 0;
                discard_line = false;
            } else if (!discard_line) {
                if (line_length < sizeof(line) - 1) {
                    line[line_length++] = (char)value;
                } else {
                    discard_line = true;
                }
            }
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_status_tick) >= pdMS_TO_TICKS(1000)) {
            (void)send_status();
            last_status_tick = now;
        }
    }
}

esp_err_t uart_serial_start(void)
{
    const uart_config_t config = {
        .baud_rate = SERIAL_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(
        SERIAL_UART_NUM, SERIAL_RX_BUFFER_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_param_config(SERIAL_UART_NUM, &config);
    if (err == ESP_OK) {
        err = uart_set_pin(SERIAL_UART_NUM, BOARD_UART_TX_GPIO,
                           BOARD_UART_RX_GPIO, UART_PIN_NO_CHANGE,
                           UART_PIN_NO_CHANGE);
    }
    if (err != ESP_OK) {
        uart_driver_delete(SERIAL_UART_NUM);
        return err;
    }

    if (xTaskCreate(uart_command_task, "uart_commands", SERIAL_TASK_STACK_SIZE,
                    NULL, 4, NULL) != pdPASS) {
        uart_driver_delete(SERIAL_UART_NUM);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UART2 lista: %d baudios, TX GPIO%d, RX GPIO%d",
             SERIAL_BAUD_RATE, BOARD_UART_TX_GPIO, BOARD_UART_RX_GPIO);
    return ESP_OK;
}
