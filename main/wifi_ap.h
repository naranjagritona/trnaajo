#ifndef WIFI_AP_H
#define WIFI_AP_H

#include "esp_err.h"

#define WIFI_AP_SSID "¿sos un capito?"
#define WIFI_AP_HOSTNAME "esp32-control"
#define WIFI_AP_DASHBOARD_URL "http://" WIFI_AP_HOSTNAME ".local"

esp_err_t wifi_ap_start(void);

#endif