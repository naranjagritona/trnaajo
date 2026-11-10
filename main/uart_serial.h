#ifndef UART_SERIAL_H
#define UART_SERIAL_H

#include "esp_err.h"

// Inicia UART2 y la tarea que recibe comandos del dashboard.
esp_err_t uart_serial_start(void);

#endif
