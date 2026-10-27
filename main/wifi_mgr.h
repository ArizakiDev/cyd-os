#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

typedef void (*wifi_mgr_ip_cb_t)(void);

/* Init NVS + reseau + Wi-Fi. Reconnecte tout seul si des identifiants sont sauvegardes. */
esp_err_t wifi_mgr_init(wifi_mgr_ip_cb_t on_got_ip);
esp_err_t wifi_mgr_connect(const char *ssid, const char *pass, bool save);
bool      wifi_mgr_get_ip(char *buf, size_t len);   /* false si non connecte */
