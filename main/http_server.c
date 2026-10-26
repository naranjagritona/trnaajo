#include "http_server.h"

#include <stdbool.h>
#include <stdio.h>

#include "esp_http_server.h"

#include "led_control.h"
#include "motor_control.h"

static httpd_handle_t server;
static const bool turn_led_on = true;
static const bool turn_led_off = false;

typedef struct {
    const char *path;
    const char *content_type;
} static_file_t;

static const static_file_t styles_file = {
    .path = "/spiffs/styles.css",
    .content_type = "text/css; charset=utf-8",
};

static const static_file_t script_file = {
    .path = "/spiffs/app.js",
    .content_type = "application/javascript; charset=utf-8",
};

static esp_err_t root_get_handler(httpd_req_t *req)
{
    FILE *page = fopen("/spiffs/index.html", "r");
    if (page == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No se pudo abrir index.html en SPIFFS");
    }

    char buffer[256];
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), page)) > 0) {
        esp_err_t err = httpd_resp_send_chunk(req, buffer, bytes_read);
        if (err != ESP_OK) {
            fclose(page);
            return err;
        }
    }

    bool read_error = ferror(page) != 0;
    fclose(page);
    if (read_error) {
        return ESP_FAIL;
    }

    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t static_file_get_handler(httpd_req_t *req)
{
    const static_file_t *asset = req->user_ctx;
    if (asset == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recurso no configurado");
    }

    FILE *file = fopen(asset->path, "r");
    if (file == NULL) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Recurso no encontrado en SPIFFS");
    }

    char buffer[512];
    httpd_resp_set_type(req, asset->content_type);
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        esp_err_t err = httpd_resp_send_chunk(req, buffer, bytes_read);
        if (err != ESP_OK) {
            fclose(file);
            return err;
        }
    }

    bool read_error = ferror(file) != 0;
    fclose(file);
    if (read_error) {
        return ESP_FAIL;
    }

    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t led_post_handler(httpd_req_t *req)
{
    const bool *desired_state = req->user_ctx;
    if (desired_state == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Estado LED no configurado");
    }

    esp_err_t err = led_control_set(*desired_state);
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No se pudo cambiar el LED");
    }

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t motor_post_handler(httpd_req_t *req)
{
    const bool *desired_state = req->user_ctx;
    if (desired_state == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Estado del motor no configurado");
    }

    esp_err_t err = motor_control_set(*desired_state);
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No se pudo cambiar el motor");
    }

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static const httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
};

static const httpd_uri_t styles_uri = {
    .uri = "/styles.css",
    .method = HTTP_GET,
    .handler = static_file_get_handler,
    .user_ctx = (void *)&styles_file,
};

static const httpd_uri_t script_uri = {
    .uri = "/app.js",
    .method = HTTP_GET,
    .handler = static_file_get_handler,
    .user_ctx = (void *)&script_file,
};

static const httpd_uri_t led_on_uri = {
    .uri = "/led/on",
    .method = HTTP_POST,
    .handler = led_post_handler,
    .user_ctx = (void *)&turn_led_on,
};

static const httpd_uri_t led_off_uri = {
    .uri = "/led/off",
    .method = HTTP_POST,
    .handler = led_post_handler,
    .user_ctx = (void *)&turn_led_off,
};

static const httpd_uri_t motor_on_uri = {
    .uri = "/motor/on",
    .method = HTTP_POST,
    .handler = motor_post_handler,
    .user_ctx = (void *)&turn_led_on,
};

static const httpd_uri_t motor_off_uri = {
    .uri = "/motor/off",
    .method = HTTP_POST,
    .handler = motor_post_handler,
    .user_ctx = (void *)&turn_led_off,
};

esp_err_t http_server_start(void)
{
    if (server != NULL) {
        return ESP_OK;
    }

    const httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        return err;
    }

    err = httpd_register_uri_handler(server, &root_uri);
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(server, &styles_uri);
    }
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(server, &script_uri);
    }
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(server, &led_on_uri);
    }
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(server, &led_off_uri);
    }
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(server, &motor_on_uri);
    }
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(server, &motor_off_uri);
    }
    if (err != ESP_OK) {
        httpd_stop(server);
        server = NULL;
        return err;
    }

    return ESP_OK;
}