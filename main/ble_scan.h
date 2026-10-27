#pragma once
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    char   addr[18];
    char   name[20];
    int8_t rssi;
} ble_dev_t;

esp_err_t ble_scan_init(void);
/* Scan bloquant. Retourne le nombre d'appareils (ou -1 si erreur). */
int ble_scan_run(ble_dev_t *out, int max, int duration_ms);
