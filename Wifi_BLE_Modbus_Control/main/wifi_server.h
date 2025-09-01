#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"

// Initialize WiFi in AP+STA mode
esp_err_t wifi_init_softap(void);

// Start HTTP server
esp_err_t start_webserver(void);

// Stop HTTP server
void stop_webserver(void);


void start_dns_server(void);
#endif