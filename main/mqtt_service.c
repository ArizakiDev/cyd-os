#include "mqtt_service.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "board.h"

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t s_cli;
static volatile bool s_up;

#define TOPIC_STATUS CONFIG_CYD_MQTT_PREFIX "/status"
#define TOPIC_LED    CONFIG_CYD_MQTT_PREFIX "/led/set"   /* payload : "r,g,b" (0-255) */

static uint8_t c255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static void on_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        s_up = true;
        esp_mqtt_client_subscribe(s_cli, TOPIC_LED, 0);
        ESP_LOGI(TAG, "connecte");
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_up = false;
        break;
    case MQTT_EVENT_DATA: {
        char tmp[32];
        int n = e->data_len < 31 ? e->data_len : 31;
        memcpy(tmp, e->data, n);
        tmp[n] = 0;
        int r, g, b;
        if (sscanf(tmp, "%d,%d,%d", &r, &g, &b) == 3) board_led_set(c255(r), c255(g), c255(b));
        break;
    }
    default:
        break;
    }
}

static void publish_task(void *arg)
{
    char js[160];
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        if (!s_up) continue;
        wifi_ap_record_t ap;
        int rssi = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : 0;
        snprintf(js, sizeof js, "{\"heap\":%lu,\"ldr\":%d,\"rssi\":%d}",
                 (unsigned long)esp_get_free_heap_size(), board_ldr_read_percent(), rssi);
        esp_mqtt_client_publish(s_cli, TOPIC_STATUS, js, 0, 0, 0);
    }
}

void mqtt_service_start(void)
{
    if (CONFIG_CYD_MQTT_URI[0] == 0) {
        ESP_LOGI(TAG, "MQTT desactive (URI vide)");
        return;
    }
    esp_mqtt_client_config_t cfg = { .broker.address.uri = CONFIG_CYD_MQTT_URI };
    s_cli = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_cli, ESP_EVENT_ANY_ID, on_mqtt, NULL);
    esp_mqtt_client_start(s_cli);
    xTaskCreate(publish_task, "mqtt_pub", 3072, NULL, 4, NULL);
}
