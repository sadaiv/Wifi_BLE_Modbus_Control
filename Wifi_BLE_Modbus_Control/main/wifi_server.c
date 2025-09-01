#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_netif.h"
#include "esp_http_server.h"

static const char *TAG = "web";

// If you don't want a separate .inl file, you can paste the HTML directly into a C string.
// For neat builds, generate index_html.inl from index.html using a pre-build step or embed with file-to-array.

// --- HTTP Handlers ---
 esp_err_t root_get_handler(httpd_req_t *req)
{
    FILE *f = fopen("/spiffs/index.html", "r");
    if (f == NULL) {
        ESP_LOGE("HTTP", "Failed to open /spiffs/index.html");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "text/html; charset=utf-8");

    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, n);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // end response
    return ESP_OK;
}

esp_err_t info_get_handler(httpd_req_t *req)
{
    char buf[160];
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    snprintf(buf, sizeof(buf),
             "{\"model\":\"ESP32\",\"idf\":\"%s\",\"heap_free\":%u}",
             esp_get_idf_version(), (unsigned)esp_get_free_heap_size());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

esp_err_t gpio_get_handler(httpd_req_t *req)
{
    char param[16];
    int pin = 2, state = -1; // default LED pin
    if (httpd_req_get_url_query_len(req) > 0) {
        char *qry = malloc(httpd_req_get_url_query_len(req) + 1);
        if (qry) {
            httpd_req_get_url_query_str(req, qry, httpd_req_get_url_query_len(req) + 1);
            if (httpd_query_key_value(qry, "pin", param, sizeof(param)) == ESP_OK) pin = atoi(param);
            if (httpd_query_key_value(qry, "state", param, sizeof(param)) == ESP_OK) state = atoi(param);
            free(qry);
        }
    }
    if (state == 0 || state == 1) {
        //gpio_reset_pin(pin);
       // gpio_set_direction(pin, GPIO_MODE_OUTPUT);
       // gpio_set_level(pin, state);
    }
    char out[64];
    snprintf(out, sizeof(out), "{\"pin\":%d,\"state\":%d}", pin, state);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);
    return ESP_OK;
}

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
      ESP_LOGE("HTTP", "starting web server");
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &root);

        httpd_uri_t info = { .uri = "/api/info", .method = HTTP_GET, .handler = info_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &info);

        httpd_uri_t gpio = { .uri = "/api/gpio", .method = HTTP_GET, .handler = gpio_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &gpio);
    }
    return server;
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "esp32-demo",
            .ssid_len = 0,
            .channel = 1,
            .password = "esp32demo",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    if (strlen((char *)wifi_config.ap.password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP started. SSID:%s password:%s", wifi_config.ap.ssid, wifi_config.ap.password);
}

// void app_main(void)
// {
//     ESP_ERROR_CHECK(nvs_flash_init());
//     wifi_init_softap();
//     start_webserver();
// }