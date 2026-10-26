#include "esp_err.h"
#include "esp_log.h"

#include "http_server.h"
#include "ir_remote.h"
#include "led_control.h"
#include "motor_control.h"
#include "spiffs_fs.h"
#include "wifi_ap.h"

static const char *TAG = "APP";

void app_main(void)
{
	ESP_ERROR_CHECK(led_control_init());
	ESP_ERROR_CHECK(motor_control_init());
	ESP_ERROR_CHECK(ir_remote_start());
	ESP_ERROR_CHECK(spiffs_fs_init());
	ESP_ERROR_CHECK(wifi_ap_start());
	ESP_ERROR_CHECK(http_server_start());

	ESP_LOGI(TAG, "Conéctate a la red ESP32-LED y abre http://192.168.4.1");
}
