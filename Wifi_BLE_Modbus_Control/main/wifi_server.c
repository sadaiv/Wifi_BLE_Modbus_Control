#include <stdio.h>
#include <string.h>
#include <ctype.h>       // <-- add this
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_http_server.h"


static const char *TAG = "WEB";

static char sta_ip_str[16] = "0.0.0.0";
static bool wifi_connected = false;
static char custom_status[32] = "Idle";

/* ===== Helpers ===== */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        strcpy(sta_ip_str, "0.0.0.0");
        ESP_LOGI(TAG, "Disconnected from Wi-Fi");
    }
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        snprintf(sta_ip_str, sizeof(sta_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
        ESP_LOGI(TAG, "Got IP: %s", sta_ip_str);
    }
}

/* ===== API Handlers ===== */

static esp_err_t root_get_handler(httpd_req_t *req)
{
    FILE* f = fopen("/spiffs/index.html", "r");
    if (f == NULL) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = malloc(filesize + 1);
    if (!buffer) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    fread(buffer, 1, filesize, f);
    buffer[filesize] = '\0';
    fclose(f);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, buffer, filesize);

    free(buffer);
    return ESP_OK;
}


static esp_err_t status_get_handler(httpd_req_t *req)
{
    char resp[128];
    snprintf(resp, sizeof(resp),
             "{\"wifi_status\":\"%s\",\"ip_address\":\"%s\",\"custom_status\":\"%s\"}",
             wifi_connected ? "connected" : "disconnected",
             sta_ip_str, custom_status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}


static void url_decode(const char *src, char *dst, int dst_size)
{
    char a, b;
    while (*src && --dst_size) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}
 
static esp_err_t networks_get_handler(httpd_req_t *req)
{
    wifi_scan_config_t scanConf = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
    };

    esp_wifi_scan_start(&scanConf, true);

    uint16_t apCount = 0;
    esp_wifi_scan_get_ap_num(&apCount);
    wifi_ap_record_t *ap_info = malloc(apCount * sizeof(wifi_ap_record_t));
    esp_wifi_scan_get_ap_records(&apCount, ap_info);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\"networks\":[");

    for (int i = 0; i < apCount; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "{\"ssid\":\"%s\",\"rssi\":%d}%s",
                 (char*)ap_info[i].ssid,
                 ap_info[i].rssi,
                 (i == apCount-1) ? "" : ",");
        httpd_resp_sendstr_chunk(req, buf);
    }

    httpd_resp_sendstr_chunk(req, "]}");
    httpd_resp_sendstr_chunk(req, NULL);
    free(ap_info);
    return ESP_OK;
}

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
esp_err_t connect_post_handler(httpd_req_t *req)
{
    char buf[128];
    int ret, remaining = req->content_len;

    // Read body
    int received = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf) - 1));
    if (received <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[received] = '\0';

    ESP_LOGI(TAG, "Received connect data: %s", buf);

    // Parse ssid=...&password=...
    char ssid[33] = {0};
    char password[65] = {0};

    char *ssid_ptr = strstr(buf, "ssid=");
    char *pass_ptr = strstr(buf, "password=");

    if (ssid_ptr) {
        ssid_ptr += 5; // skip "ssid="
        char *amp = strchr(ssid_ptr, '&');
        if (amp) *amp = '\0';
        url_decode(ssid_ptr, ssid, sizeof(ssid));  // optional helper for %20 etc
    }

    if (pass_ptr) {
        pass_ptr += 9; // skip "password="
        url_decode(pass_ptr, password, sizeof(password));
    }

    ESP_LOGI(TAG, "SSID: %s, PASS: %s", ssid, password);

    // Configure STA
    wifi_config_t sta_config = {0};
    strncpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    strncpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Send JSON response
    const char *resp = "{\"status\":\"success\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}


/* ===== Webserver ===== */
httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
                httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root);

        httpd_uri_t status = { .uri="/api/status_info", .method=HTTP_GET, .handler=status_get_handler };
        httpd_register_uri_handler(server, &status);

        httpd_uri_t networks = { .uri="/api/networks", .method=HTTP_GET, .handler=networks_get_handler };
        httpd_register_uri_handler(server, &networks);

       httpd_uri_t connect_uri = {
    .uri = "/api/connect",
    .method = HTTP_POST,
    .handler = connect_post_handler,
    .user_ctx = NULL
};
        httpd_register_uri_handler(server, &connect_uri);
    }
    return server;
}

/* ===== Wi-Fi AP for captive portal ===== */
void wifi_init_softap(void)
{
      // Initialize netif and event loop ONCE
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create both default interfaces
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    // Init Wi-Fi driver once
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Configure AP
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP32-Setup",
            .ssid_len = strlen("ESP32-Setup"),
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    strcpy((char *)ap_config.ap.password, "12345678");

    // Configure STA (empty for now, can be set later from captive portal form)
    wifi_config_t sta_config = {0};

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi started. AP SSID: ESP32-Setup, Password: 12345678");
}
