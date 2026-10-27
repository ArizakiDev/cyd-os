#include "web_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board.h"
#include "wifi_mgr.h"

static const char *TAG = "web";

extern const char index_html_start[] asm("_binary_index_html_start");

static bool authorized(httpd_req_t *req)
{
    char tok[64] = { 0 };
    if (httpd_req_get_hdr_value_str(req, "X-Token", tok, sizeof tok) != ESP_OK) return false;
    return strcmp(tok, CONFIG_CYD_WEB_TOKEN) == 0;
}

static esp_err_t index_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html_start, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_get(httpd_req_t *req)
{
    char ip[16], js[200];
    wifi_mgr_get_ip(ip, sizeof ip);
    wifi_ap_record_t ap;
    int rssi = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : 0;
    snprintf(js, sizeof js,
             "{\"idf\":\"%s\",\"heap\":%lu,\"uptime\":%d,\"ldr\":%d,\"rssi\":%d,\"ip\":\"%s\"}",
             esp_get_idf_version(), (unsigned long)esp_get_free_heap_size(),
             (int)(esp_timer_get_time() / 1000000), board_ldr_read_percent(), rssi, ip);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, js);
}

static int clamp255(const char *s) { int v = atoi(s); return v < 0 ? 0 : (v > 255 ? 255 : v); }

static esp_err_t led_post(httpd_req_t *req)
{
    if (!authorized(req)) return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "token invalide");
    char q[64], r[8] = "0", g[8] = "0", b[8] = "0";
    if (httpd_req_get_url_query_str(req, q, sizeof q) == ESP_OK) {
        httpd_query_key_value(q, "r", r, sizeof r);
        httpd_query_key_value(q, "g", g, sizeof g);
        httpd_query_key_value(q, "b", b, sizeof b);
    }
    board_led_set(clamp255(r), clamp255(g), clamp255(b));
    return httpd_resp_sendstr(req, "OK");
}

static esp_err_t ota_post(httpd_req_t *req)
{
    if (!authorized(req)) return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "token invalide");

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "pas de partition OTA");
    if ((size_t)req->content_len > part->size)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "firmware trop gros");

    esp_ota_handle_t h;
    if (esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &h) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_begin");

    char buf[1024];
    int remaining = req->content_len;
    while (remaining > 0) {
        int want = remaining < (int)sizeof buf ? remaining : (int)sizeof buf;
        int r = httpd_req_recv(req, buf, want);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0 || esp_ota_write(h, buf, r) != ESP_OK) {
            esp_ota_abort(h);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ecriture echouee");
        }
        remaining -= r;
    }
    if (esp_ota_end(h) != ESP_OK || esp_ota_set_boot_partition(part) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "firmware invalide");

    httpd_resp_sendstr(req, "OK, redemarrage...");
    ESP_LOGI(TAG, "OTA reussie, redemarrage");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

void web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 6144;
    httpd_handle_t srv = NULL;
    if (httpd_start(&srv, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start a echoue");
        return;
    }
    const httpd_uri_t uris[] = {
        { .uri = "/",           .method = HTTP_GET,  .handler = index_get },
        { .uri = "/api/status", .method = HTTP_GET,  .handler = status_get },
        { .uri = "/api/led",    .method = HTTP_POST, .handler = led_post },
        { .uri = "/ota",        .method = HTTP_POST, .handler = ota_post },
    };
    for (size_t i = 0; i < sizeof uris / sizeof uris[0]; i++) httpd_register_uri_handler(srv, &uris[i]);
    ESP_LOGI(TAG, "serveur web demarre");
}
