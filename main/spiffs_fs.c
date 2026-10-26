#include "spiffs_fs.h"

#include <stdbool.h>

#include "esp_log.h"
#include "esp_spiffs.h"

static const char *TAG = "SPIFFS_FS";
static bool mounted;

/*
 * Monta la partición SPIFFS para poder leer archivos web estáticos.
 * Estos archivos son la interfaz HTML, CSS y JS del panel de control.
 */
esp_err_t spiffs_fs_init(void)
{
    if (mounted) {
        return ESP_OK;
    }

    const esp_vfs_spiffs_conf_t config = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false,
    };

    esp_err_t err = esp_vfs_spiffs_register(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo montar SPIFFS (err=0x%x). Verifica que se haya grabado la imagen de la partición.", err);
        return err;
    }

    size_t total_bytes = 0;
    size_t used_bytes = 0;
    err = esp_spiffs_info("storage", &total_bytes, &used_bytes);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo obtener el espacio de SPIFFS (err=0x%x)", err);
        esp_vfs_spiffs_unregister("storage");
        return err;
    }

    mounted = true;
    ESP_LOGI(TAG, "SPIFFS montado: %u/%u bytes usados", (unsigned)used_bytes, (unsigned)total_bytes);
    return ESP_OK;
}
