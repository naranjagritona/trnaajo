#include "ir_remote.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "led_control.h"
#include "motor_control.h"

#define IR_RECEIVER_GPIO GPIO_NUM_23
#define IR_RESOLUTION_HZ 1000000
#define IR_SYMBOL_BUFFER_SIZE 80

static const char *TAG = "IR_REMOTE";
static rmt_channel_handle_t rx_channel;
static QueueHandle_t rx_queue;
static rmt_symbol_word_t rx_symbols[IR_SYMBOL_BUFFER_SIZE];
static const rmt_receive_config_t receive_config = {
    .signal_range_min_ns = 1000,
    .signal_range_max_ns = 12000000,
};

static bool near(uint32_t value, uint32_t target, uint32_t tolerance)
{
    return (value > (target - tolerance)) &&
           (value < (target + tolerance));
}

static const char *get_button_name(uint32_t code)
{
    switch (code) {
        case 0xBA45FF00: return "CH-";
        case 0xE619FF00: return "CH";
        case 0xF609FF00: return "CH+";

        case 0xBF40FF00: return "PREV";
        case 0xBC43FF00: return "PLAY";
        case 0xEA15FF00: return "NEXT";

        case 0xF807FF00: return "VOL-";
        case 0xF20DFF00: return "EQ";

        case 0xF30CFF00: return "1";
        case 0xE718FF00: return "2";
        case 0xA15EFF00: return "3";

        case 0xF708FF00: return "4";
        case 0xE31CFF00: return "5";
        case 0xA55AFF00: return "6";

        case 0xBD42FF00: return "7";
        case 0xB54AFF00: return "8";
        case 0xAD52FF00: return "9";

        case 0xB946FF00: return "100+";
        case 0xB847FF00: return "200+";

        default:
            return "DESCONOCIDO";
    }
}

static bool ir_receive_done_callback(rmt_channel_handle_t channel,
                                     const rmt_rx_done_event_data_t *event,
                                     void *user_data)
{
    (void)channel;
    (void)user_data;
    BaseType_t higher_priority_task_woken = pdFALSE;
    xQueueSendFromISR(rx_queue, &event->num_symbols, &higher_priority_task_woken);
    return higher_priority_task_woken == pdTRUE;
}

static bool decode_nec_command(size_t symbol_count, uint32_t *code)
{
    if (symbol_count < 33 || symbol_count > IR_SYMBOL_BUFFER_SIZE) {
        return false;
    }

    size_t leader = 0;
    while (leader < symbol_count &&
           !(near(rx_symbols[leader].duration0, 9000, 1000) &&
             near(rx_symbols[leader].duration1, 4500, 1000))) {
        leader++;
    }
    if (leader == symbol_count || leader + 32 >= symbol_count) {
        return false;
    }

    uint32_t frame = 0;
    for (size_t bit = 0; bit < 32; bit++) {
        const rmt_symbol_word_t *symbol = &rx_symbols[leader + bit + 1];
        if (!near(symbol->duration0, 600, 300)) {
            return false;
        }

        if (near(symbol->duration1, 600, 250)) {
            /* bit 0 */
        } else if (near(symbol->duration1, 1600, 400)) {
            frame |= (uint32_t)1U << bit;
        } else {
            return false;
        }
    }

    uint8_t received_command = (uint8_t)((frame >> 16) & 0xff);
    uint8_t command_inverse = (uint8_t)((frame >> 24) & 0xff);
    if ((uint8_t)~received_command != command_inverse) {
        return false;
    }

    *code = frame;
    return true;
}

static void ir_remote_task(void *argument)
{
    (void)argument;

    for (;;) {
        size_t symbol_count = 0;
        if (xQueueReceive(rx_queue, &symbol_count, portMAX_DELAY) == pdTRUE) {
            uint32_t code;
            if (decode_nec_command(symbol_count, &code)) {
                ESP_LOGI(TAG, "Comando NEC recibido: 0x%08" PRIX32 " (%s)",
                         code, get_button_name(code));
                if (code == 0xF30CFF00) {
                    esp_err_t err = led_control_toggle();
                    if (err == ESP_OK) {
                        ESP_LOGI(TAG, "Tecla 1: luces GPIO22 %s",
                                 led_control_is_on() ? "encendidas" : "apagadas");
                    } else {
                        ESP_LOGE(TAG, "No se pudo cambiar GPIO22: 0x%x", err);
                    }
                } else if (code == 0xE718FF00) {
                    esp_err_t err = motor_control_toggle();
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "No se pudo cambiar GPIO16: 0x%x", err);
                    }
                }
            }

            esp_err_t err = rmt_receive(rx_channel, rx_symbols, sizeof(rx_symbols), &receive_config);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "No se pudo reiniciar la recepción IR: 0x%x", err);
                vTaskDelay(pdMS_TO_TICKS(100));
                rmt_receive(rx_channel, rx_symbols, sizeof(rx_symbols), &receive_config);
            }
        }
    }
}

esp_err_t ir_remote_start(void)
{
    rx_queue = xQueueCreate(1, sizeof(size_t));
    if (rx_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const rmt_rx_channel_config_t channel_config = {
        .gpio_num = IR_RECEIVER_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = IR_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .flags.invert_in = false,
        .flags.with_dma = false,
    };

    esp_err_t err = rmt_new_rx_channel(&channel_config, &rx_channel);
    if (err != ESP_OK) {
        return err;
    }

    const rmt_rx_event_callbacks_t callbacks = {
        .on_recv_done = ir_receive_done_callback,
    };
    err = rmt_rx_register_event_callbacks(rx_channel, &callbacks, NULL);
    if (err != ESP_OK) {
        return err;
    }

    err = rmt_enable(rx_channel);
    if (err != ESP_OK) {
        return err;
    }

    err = rmt_receive(rx_channel, rx_symbols, sizeof(rx_symbols), &receive_config);
    if (err != ESP_OK) {
        return err;
    }

    if (xTaskCreate(ir_remote_task, "ir_remote", 3072, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Receptor IR listo en GPIO%d (NEC)", IR_RECEIVER_GPIO);
    return ESP_OK;
}