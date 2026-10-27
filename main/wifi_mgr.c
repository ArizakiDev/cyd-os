#include "wifi_mgr.h"
#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_mgr";
static wifi_mgr_ip_cb_t s_cb;
static volatile bool s_connected, s_want;
static int s_retry;
static char s_ip[16];

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            if (s_want) esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            s_connected = false;
            s_ip[0] = 0;
            if (s_want && s_retry++ < 10) esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof s_ip, IPSTR, IP2STR(&e->ip_info.ip));
        s_connected = true;
        s_retry = 0;
        ESP_LOGI(TAG, "IP: %s", s_ip);
        if (s_cb) s_cb();
    }
}

static bool load_creds(char *ssid, size_t sl, char *pass, size_t pl)
{
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READONLY, &h) != ESP_OK) return false;
    esp_err_t a = nvs_get_str(h, "ssid", ssid, &sl);
    esp_err_t b = nvs_get_str(h, "pass", pass, &pl);
    nvs_close(h);
    return a == ESP_OK && b == ESP_OK;
}

static void save_creds(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, "ssid", ssid);
    nvs_set_str(h, "pass", pass);
    nvs_commit(h);
    nvs_close(h);
}

static void fill_cfg(wifi_config_t *cfg, const char *ssid, const char *pass)
{
    memset(cfg, 0, sizeof *cfg);
    strlcpy((char *)cfg->sta.ssid, ssid, sizeof cfg->sta.ssid);
    strlcpy((char *)cfg->sta.password, pass, sizeof cfg->sta.password);
    cfg->sta.pmf_cfg.capable = true;
}

esp_err_t wifi_mgr_init(wifi_mgr_ip_cb_t on_got_ip)
{
    s_cb = on_got_ip;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    char ssid[33], pass[65];
    if (load_creds(ssid, sizeof ssid, pass, sizeof pass)) {
        wifi_config_t wc;
        fill_cfg(&wc, ssid, pass);
        esp_wifi_set_config(WIFI_IF_STA, &wc);
        s_want = true;
        ESP_LOGI(TAG, "identifiants sauvegardes trouves: %s", ssid);
    }
    return esp_wifi_start();
}

esp_err_t wifi_mgr_connect(const char *ssid, const char *pass, bool save)
{
    wifi_config_t wc;
    fill_cfg(&wc, ssid, pass);
    s_want = true;
    s_retry = 0;
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    if (save) save_creds(ssid, pass);
    return esp_wifi_connect();
}

bool wifi_mgr_get_ip(char *buf, size_t len)
{
    if (!s_connected) { buf[0] = 0; return false; }
    strlcpy(buf, s_ip, len);
    return true;
}
