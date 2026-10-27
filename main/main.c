#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_netif_sntp.h"
#include "esp_ota_ops.h"
#include "board.h"
#include "ble_scan.h"
#include "mqtt_service.h"
#include "ui.h"
#include "web_server.h"
#include "wifi_mgr.h"

/* Appele a chaque fois qu'on obtient une IP : demarre les services reseau (une seule fois). */
static void on_got_ip(void)
{
    static bool done;
    if (done) return;
    done = true;

    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);   /* Belgique / France */
    tzset();
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&sntp);

    web_server_start();
    mqtt_service_start();
}

void app_main(void)
{
    ESP_ERROR_CHECK(board_init());
    ESP_ERROR_CHECK(board_display_init());
    ESP_ERROR_CHECK(wifi_mgr_init(on_got_ip));
    ESP_ERROR_CHECK(ble_scan_init());

    if (lvgl_port_lock(0)) {
        ui_create();
        lvgl_port_unlock();
    }
    board_backlight_set(80);

    /* Tout a demarre : on valide ce firmware (sinon rollback au prochain reboot apres un OTA). */
    esp_ota_mark_app_valid_cancel_rollback();
}
