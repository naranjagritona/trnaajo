#include "esp_err.h"
#include "esp_log.h"

#include "http_server.h"
#include "dht11_sensor.h"
#include "ir_remote.h"
#include "led_control.h"
#include "machine_control.h"
#include "motor_control.h"
#include "spiffs_fs.h"
#include "wifi_ap.h"

static const char *TAG = "APP";

/*
 * Punto de entrada de la aplicación.
 * Inicializa los periféricos del proyecto en el orden correcto
 * y deja el sistema listo para recibir peticiones web y señales IR.
 */
void app_main(void)
{
	ESP_ERROR_CHECK(led_control_init());
	ESP_ERROR_CHECK(motor_control_init());
	ESP_ERROR_CHECK(machine_control_init());
	ESP_ERROR_CHECK(dht11_sensor_start());
	ESP_ERROR_CHECK(ir_remote_start());
	ESP_ERROR_CHECK(spiffs_fs_init());
	ESP_ERROR_CHECK(wifi_ap_start());
	ESP_ERROR_CHECK(http_server_start());

	ESP_LOGI(TAG, "Conéctate a la red %s y abre %s", WIFI_AP_SSID, WIFI_AP_DASHBOARD_URL);
}
