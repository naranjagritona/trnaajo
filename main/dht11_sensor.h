#ifndef DHT11_SENSOR_H
#define DHT11_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t dht11_sensor_start(void);
bool dht11_sensor_get_reading(int16_t *temperature_tenths, int16_t *humidity_tenths);

#endif
