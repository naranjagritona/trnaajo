idf_component_register(
    SRCS
        "main.c"
        "led_control.c"
        "machine_control.c"
        "motor_control.c"
        "ir_remote.c"
        "wifi_ap.c"
        "spiffs_fs.c"
        "http_server.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_event
        esp_driver_gpio
        esp_driver_rmt
        esp_http_server
        mdns
        esp_netif
        esp_timer
        esp_wifi
        nvs_flash
        spiffs
)

    spiffs_create_partition_image(storage "${CMAKE_CURRENT_LIST_DIR}/../spiffs" FLASH_IN_PROJECT)
