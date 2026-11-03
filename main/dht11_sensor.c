#include "dht11_sensor.h"

#include <stdbool.h>
#include <stdint.h>

#include "dht.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

#include "board_pins.h"

#define DHT11_SAMPLE_INTERVAL_MS 2000
#define DHT11_TASK_STACK_SIZE 3072

static const char *TAG = "DHT11";
static portMUX_TYPE reading_lock = portMUX_INITIALIZER_UNLOCKED;
static int16_t last_temperature_tenths;
static int16_t last_humidity_tenths;
static bool reading_available;

bool dht11_sensor_get_reading(int16_t *temperature_tenths, int16_t *humidity_tenths)
{
    if (temperature_tenths == NULL || humidity_tenths == NULL) {
        return false;
    }

    portENTER_CRITICAL(&reading_lock);
    bool available = reading_available;
    if (available) {
        *temperature_tenths = last_temperature_tenths;
        *humidity_tenths = last_humidity_tenths;
    }
    portEXIT_CRITICAL(&reading_lock);
    return available;
}

static void sensor_task(void *argument)
{
    (void)argument;
    bool error_reported = false;

    for (;;) {
        int16_t humidity_tenths;
        int16_t temperature_tenths;
        esp_err_t err = dht_read_data(
            DHT_TYPE_DHT11, BOARD_DHT11_GPIO, &humidity_tenths, &temperature_tenths);
        if (err == ESP_OK) {
            portENTER_CRITICAL(&reading_lock);
            last_temperature_tenths = temperature_tenths;
            last_humidity_tenths = humidity_tenths;
            reading_available = true;
            portEXIT_CRITICAL(&reading_lock);

            ESP_LOGI(TAG, "Temperatura: %d.%d C | Humedad: %d.%d%%",
                     temperature_tenths / 10, temperature_tenths % 10,
                     humidity_tenths / 10, humidity_tenths % 10);
            error_reported = false;
        } else if (!error_reported) {
            ESP_LOGE(TAG, "No se pudo leer el DHT11: %s", esp_err_to_name(err));
            error_reported = true;
        }

        vTaskDelay(pdMS_TO_TICKS(DHT11_SAMPLE_INTERVAL_MS));
    }
}

esp_err_t dht11_sensor_start(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOARD_DHT11_GPIO,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }

    err = gpio_set_level(BOARD_DHT11_GPIO, 1);
    if (err != ESP_OK) {
        return err;
    }

    if (xTaskCreate(sensor_task, "dht11_sensor", DHT11_TASK_STACK_SIZE,
                    NULL, 4, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "DHT11 listo en GPIO%d; lectura cada %u ms",
             BOARD_DHT11_GPIO, DHT11_SAMPLE_INTERVAL_MS);
    return ESP_OK;
}
