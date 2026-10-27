#include "ble_scan.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"   /* ble_hs_util_ensure_addr() */

static const char *TAG = "ble";
static ble_dev_t *s_out;
static int s_max, s_cnt;
static SemaphoreHandle_t s_done;
static volatile bool s_synced;
static uint8_t s_own_type;

static int gap_cb(struct ble_gap_event *ev, void *arg)
{
    switch (ev->type) {
    case BLE_GAP_EVENT_DISC: {
        if (!s_out) break;
        const uint8_t *v = ev->disc.addr.val;
        char addr[18];
        snprintf(addr, sizeof addr, "%02X:%02X:%02X:%02X:%02X:%02X", v[5], v[4], v[3], v[2], v[1], v[0]);

        int idx = -1;
        for (int i = 0; i < s_cnt; i++) {
            if (!strcmp(s_out[i].addr, addr)) { idx = i; break; }
        }
        if (idx < 0) {
            if (s_cnt >= s_max) break;
            idx = s_cnt++;
            strlcpy(s_out[idx].addr, addr, sizeof s_out[idx].addr);
            s_out[idx].name[0] = 0;
        }
        s_out[idx].rssi = ev->disc.rssi;

        struct ble_hs_adv_fields f;
        if (ble_hs_adv_parse_fields(&f, ev->disc.data, ev->disc.length_data) == 0 && f.name && f.name_len) {
            size_t n = f.name_len < sizeof s_out[idx].name - 1 ? f.name_len : sizeof s_out[idx].name - 1;
            memcpy(s_out[idx].name, f.name, n);
            s_out[idx].name[n] = 0;
        }
        break;
    }
    case BLE_GAP_EVENT_DISC_COMPLETE:
        xSemaphoreGive(s_done);
        break;
    default:
        break;
    }
    return 0;
}

static void on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    ble_hs_id_infer_auto(0, &s_own_type);
    s_synced = true;
    ESP_LOGI(TAG, "BLE pret");
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_scan_init(void)
{
    s_done = xSemaphoreCreateBinary();
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) return err;
    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

int ble_scan_run(ble_dev_t *out, int max, int duration_ms)
{
    if (!s_synced) return -1;
    s_out = out; s_max = max; s_cnt = 0;
    xSemaphoreTake(s_done, 0);

    struct ble_gap_disc_params p = { .passive = 0, .filter_duplicates = 0 };
    int rc = ble_gap_disc(s_own_type, duration_ms, &p, gap_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_disc rc=%d", rc);
        return -1;
    }
    xSemaphoreTake(s_done, pdMS_TO_TICKS(duration_ms + 2000));
    return s_cnt;
}
