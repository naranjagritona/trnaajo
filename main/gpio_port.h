#ifndef GPIO_PORT_H
#define GPIO_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

static inline esp_err_t gpio_port_write(uint32_t *port_state,
                                        uint32_t mask,
                                        gpio_num_t gpio,
                                        bool high)
{
    uint32_t next_state = high ? (*port_state | mask) : (*port_state & ~mask);
    esp_err_t err = gpio_set_level(gpio, (next_state & mask) != 0);
    if (err == ESP_OK) {
        *port_state = next_state;
    }
    return err;
}

#endif
