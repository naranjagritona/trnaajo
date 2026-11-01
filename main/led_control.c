#include "led_control.h" // Declara las funciones públicas para controlar el LED.

#include "driver/gpio.h" // Proporciona la configuración y el control del GPIO.
#include "freertos/FreeRTOS.h" // Incluye tipos y herramientas base de FreeRTOS.
#include "freertos/portmacro.h" // Proporciona las macros para proteger secciones críticas.

#include "board_pins.h" // Define el número de GPIO asignado al LED.
#include "machine_state.h" // Expone el bit compartido que representa el estado del LED.

static portMUX_TYPE led_control_lock = portMUX_INITIALIZER_UNLOCKED; // Evita accesos simultáneos al cambio del LED.

static esp_err_t write_led_state(bool on) // Aplica el mismo estado al pin físico y al registro compartido.
{ // Comienza la función auxiliar.
    uint8_t state = on ? MACHINE_STATE_LED_MASK : 0b00000000U; // Convierte el booleano en el valor de bit correspondiente.
    esp_err_t err = gpio_set_level(BOARD_LED_GPIO, on); // Enciende o apaga el GPIO del LED.
    if (err != ESP_OK) { // Comprueba si ESP-IDF pudo cambiar el GPIO.
        return err; // Propaga el error sin actualizar un estado que no se aplicó.
    } // Termina la comprobación del error del GPIO.

    return machine_state_set_output(MACHINE_STATE_LED_MASK, state); // Guarda el estado del LED en la máquina común.
} // Termina la función auxiliar.

esp_err_t led_control_init(void) // Prepara el GPIO y el bit compartido del LED.
{ // Comienza la inicialización del LED.
    const gpio_config_t led_config = { // Define las opciones de configuración del pin.
        .pin_bit_mask = 1ULL << BOARD_LED_GPIO, // Selecciona únicamente el GPIO del LED.
        .mode = GPIO_MODE_OUTPUT, // Configura el GPIO como salida.
        .pull_up_en = GPIO_PULLUP_DISABLE, // Desactiva la resistencia pull-up interna.
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // Desactiva la resistencia pull-down interna.
        .intr_type = GPIO_INTR_DISABLE, // Desactiva interrupciones en el pin de salida.
    }; // Termina la configuración del GPIO.

    esp_err_t err = gpio_config(&led_config); // Aplica la configuración al GPIO.
    if (err != ESP_OK) { // Comprueba si falló la configuración del pin.
        return err; // Devuelve el error para que quien inicia el sistema lo gestione.
    } // Termina la comprobación de configuración.

    err = machine_state_configure_output(MACHINE_STATE_LED_MASK); // Marca el bit del LED como salida en la máquina de estado.
    if (err != ESP_OK) { // Comprueba si falló la configuración del estado compartido.
        return err; // Devuelve el error al código de inicialización.
    } // Termina la comprobación del estado compartido.

    return led_control_set(false); // Arranca el LED apagado y sincroniza el pin con el registro.
} // Termina la inicialización del LED.

esp_err_t led_control_set(bool on) // Fija explícitamente el LED encendido o apagado.
{ // Comienza la función para establecer el estado.
    portENTER_CRITICAL(&led_control_lock); // Evita que otra tarea cambie el LED a la vez.
    esp_err_t err = write_led_state(on); // Actualiza el GPIO y el registro compartido.
    portEXIT_CRITICAL(&led_control_lock); // Libera la protección para otras tareas.
    return err; // Informa si la actualización terminó correctamente.
} // Termina la función para establecer el estado.

esp_err_t led_control_toggle(void) // Invierte el estado actual del LED.
{ // Comienza la función para alternar el estado.
    portENTER_CRITICAL(&led_control_lock); // Protege la lectura y el cambio como una sola operación.
    bool on = machine_state_get_output(MACHINE_STATE_LED_MASK) == 0b00000000U; // Decide encenderlo si ahora está apagado.
    esp_err_t err = write_led_state(on); // Aplica el nuevo estado al GPIO y al registro compartido.
    portEXIT_CRITICAL(&led_control_lock); // Libera la protección para otras tareas.
    return err; // Informa si la inversión se realizó correctamente.
} // Termina la función para alternar el estado.

bool led_control_is_on(void) // Consulta si el estado registrado del LED está encendido.
{ // Comienza la función de consulta.
    return machine_state_get_output(MACHINE_STATE_LED_MASK) != 0b00000000U; // Devuelve true si el bit del LED está activo.
} // Termina la función de consulta.