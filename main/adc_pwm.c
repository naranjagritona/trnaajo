#include "adc_pwm.h"

#include <stdint.h>

#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_pins.h"

#define ADC_PWM_POT_CHANNEL ADC_CHANNEL_6
#define ADC_PWM_FREQUENCY 5000
#define ADC_PWM_RESOLUTION LEDC_TIMER_13_BIT
#define ADC_PWM_MAX_DUTY 8191U
#define ADC_PWM_MAX_RAW 4095
#define ADC_PWM_TASK_STACK_SIZE 3072

static const char *TAG = "ADC_PWM";
static adc_oneshot_unit_handle_t adc_handle;
static adc_channel_t pot_channel;

static void adc_pwm_task(void *arg)
{
    (void)arg;

    while (1) {
        int adc_raw = 0;
        esp_err_t err = adc_oneshot_read(adc_handle, pot_channel, &adc_raw);
        if (err == ESP_OK) {
            if (adc_raw < 0) {
                adc_raw = 0;
            } else if (adc_raw > ADC_PWM_MAX_RAW) {
                adc_raw = ADC_PWM_MAX_RAW;
            }

            uint32_t duty = ((uint32_t)adc_raw * ADC_PWM_MAX_DUTY) / ADC_PWM_MAX_RAW;

            err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
            if (err == ESP_OK) {
                err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            }
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "ADC=%d PWM=%lu", adc_raw, (unsigned long)duty);
            }
        }

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error al leer ADC o actualizar PWM: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t adc_pwm_start(void)
{
    adc_unit_t adc_unit;
    adc_unit = ADC_UNIT_1;
    pot_channel = ADC_PWM_POT_CHANNEL;
    esp_err_t err;

    const adc_oneshot_unit_init_cfg_t adc_init_cfg = {
        .unit_id = adc_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    err = adc_oneshot_new_unit(&adc_init_cfg, &adc_handle);
    if (err != ESP_OK) {
        return err;
    }

    const adc_oneshot_chan_cfg_t adc_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_oneshot_config_channel(adc_handle, pot_channel, &adc_cfg);
    if (err != ESP_OK) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
        return err;
    }

    const ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = ADC_PWM_RESOLUTION,
        .freq_hz = ADC_PWM_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
        return err;
    }

    const ledc_channel_config_t ledc_channel = {
        .gpio_num = BOARD_PWM_LED_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
        return err;
    }

    ESP_LOGI(TAG, "Inicio de prueba PWM en GPIO%d", (int)BOARD_PWM_LED_GPIO);
    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, ADC_PWM_MAX_DUTY);
    if (err == ESP_OK) {
        err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
    if (err != ESP_OK) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(750));

    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    if (err == ESP_OK) {
        err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
    if (err != ESP_OK) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
        return err;
    }

    int startup_raw = 0;
    err = adc_oneshot_read(adc_handle, pot_channel, &startup_raw);
    ESP_LOGI(TAG, "Lectura inicial ADC=%d, resultado=%s",
             startup_raw, esp_err_to_name(err));

    if (xTaskCreate(adc_pwm_task, "adc_pwm", ADC_PWM_TASK_STACK_SIZE, NULL, 5, NULL) != pdPASS) {
        ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Control iniciado: potenciómetro GPIO%d, LED PWM GPIO%d",
             (int)BOARD_POT_ADC_GPIO, (int)BOARD_PWM_LED_GPIO);
    return ESP_OK;
}