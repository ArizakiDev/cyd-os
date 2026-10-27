#include "wifi_scan.h"

int wifi_scan_run(wifi_ap_record_t *out, uint16_t max)
{
    wifi_scan_config_t sc = { 0 };
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) return -1;
    uint16_t n = max;
    if (esp_wifi_scan_get_ap_records(&n, out) != ESP_OK) return -1;
    return n;
}
