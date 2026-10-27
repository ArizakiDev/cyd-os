#pragma once
#include "esp_wifi.h"

/* Retourne le nombre de reseaux trouves (ou -1 si erreur). */
int wifi_scan_run(wifi_ap_record_t *out, uint16_t max);
