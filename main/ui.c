#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ble_scan.h"
#include "board.h"
#include "wifi_mgr.h"
#include "wifi_scan.h"

/* NB : la police par defaut de LVGL n'a pas les accents -> textes en ASCII. */

static lv_obj_t *s_info_label, *s_wifi_label, *s_ble_label, *s_ldr_bar, *s_ldr_label;
static lv_obj_t *s_auto_sw, *s_dd, *s_pass, *s_kb, *s_net_status;
static uint8_t s_rgb[3];
static volatile bool s_wifi_busy, s_ble_busy;

/* ---------- Info ---------- */
static void info_timer_cb(lv_timer_t *t)
{
    (void)t;
    esp_chip_info_t ci;
    esp_chip_info(&ci);
    uint32_t flash = 0;
    esp_flash_get_size(NULL, &flash);
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    int up = (int)(esp_timer_get_time() / 1000000);

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char tbuf[16];
    if (tm.tm_year >= 124) strftime(tbuf, sizeof tbuf, "%H:%M:%S", &tm);
    else strcpy(tbuf, "--:--:--");

    char ip[16];
    bool conn = wifi_mgr_get_ip(ip, sizeof ip);

    lv_label_set_text_fmt(s_info_label,
        "ESP-IDF : %s\n"
        "Coeurs : %d   Rev : %d\n"
        "Flash : %lu MB\n"
        "MAC : %02X:%02X:%02X:%02X:%02X:%02X\n"
        "RAM libre : %lu KB\n"
        "Uptime : %02d:%02d:%02d\n"
        "Heure : %s\n"
        "WiFi : %s",
        esp_get_idf_version(), ci.cores, ci.revision,
        (unsigned long)(flash / (1024 * 1024)),
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
        (unsigned long)(esp_get_free_heap_size() / 1024),
        up / 3600, (up / 60) % 60, up % 60,
        tbuf, conn ? ip : "non connecte");

    if (s_net_status) {
        lv_label_set_text_fmt(s_net_status, "%s", conn ? ip : "Non connecte");
    }
}

/* ---------- Scan WiFi ---------- */
static void wifi_task(void *arg)
{
    static wifi_ap_record_t recs[12];
    static char buf[1024], opts[512];
    int n = wifi_scan_run(recs, 12);

    int off = 0, ooff = 0;
    opts[0] = 0;
    if (n < 0) {
        snprintf(buf, sizeof buf, "Erreur de scan (reessaie)");
    } else {
        off += snprintf(buf, sizeof buf, "%d reseaux :\n", n);
        for (int i = 0; i < n && off < (int)sizeof buf - 64; i++) {
            const char *ssid = (const char *)recs[i].ssid;
            off += snprintf(buf + off, sizeof buf - off, "%3d dBm  %s  %s\n", recs[i].rssi,
                            recs[i].authmode == WIFI_AUTH_OPEN ? "OPEN" : "SEC ", ssid);
            if (ssid[0] && ooff < (int)sizeof opts - 40) {
                ooff += snprintf(opts + ooff, sizeof opts - ooff, "%s%s", ooff ? "\n" : "", ssid);
            }
        }
    }
    if (lvgl_port_lock(0)) {
        lv_label_set_text(s_wifi_label, buf);
        if (opts[0]) lv_dropdown_set_options(s_dd, opts);
        lvgl_port_unlock();
    }
    s_wifi_busy = false;
    vTaskDelete(NULL);
}

static void wifi_btn_cb(lv_event_t *e)
{
    (void)e;
    if (s_wifi_busy) return;
    s_wifi_busy = true;
    lv_label_set_text(s_wifi_label, "Scan en cours...");
    xTaskCreate(wifi_task, "wifi_scan", 4096, NULL, 5, NULL);
}

/* ---------- Connexion WiFi (onglet Net) ---------- */
static void ta_cb(lv_event_t *e)
{
    lv_keyboard_set_textarea(s_kb, lv_event_get_target_obj(e));
    lv_obj_remove_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
}

static void kb_cb(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_READY || c == LV_EVENT_CANCEL) lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
}

static void connect_btn_cb(lv_event_t *e)
{
    (void)e;
    char ssid[33];
    lv_dropdown_get_selected_str(s_dd, ssid, sizeof ssid);
    if (!ssid[0] || ssid[0] == '(') {
        lv_label_set_text(s_net_status, "Fais d'abord un scan (onglet Scan)");
        return;
    }
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_net_status, "Connexion...");
    wifi_mgr_connect(ssid, lv_textarea_get_text(s_pass), true);
}

/* ---------- BLE ---------- */
static void ble_task(void *arg)
{
    static ble_dev_t devs[12];
    static char buf[1024];
    int n = ble_scan_run(devs, 12, 4000);

    int off = 0;
    if (n < 0) {
        snprintf(buf, sizeof buf, "Erreur BLE (pas pret ?)");
    } else {
        off += snprintf(buf, sizeof buf, "%d appareils :\n", n);
        for (int i = 0; i < n && off < (int)sizeof buf - 64; i++) {
            off += snprintf(buf + off, sizeof buf - off, "%4d %s %s\n", devs[i].rssi, devs[i].addr,
                            devs[i].name);
        }
    }
    if (lvgl_port_lock(0)) {
        lv_label_set_text(s_ble_label, buf);
        lvgl_port_unlock();
    }
    s_ble_busy = false;
    vTaskDelete(NULL);
}

static void ble_btn_cb(lv_event_t *e)
{
    (void)e;
    if (s_ble_busy) return;
    s_ble_busy = true;
    lv_label_set_text(s_ble_label, "Scan BLE (4 s)...");
    xTaskCreate(ble_task, "ble_scan", 4096, NULL, 5, NULL);
}

/* ---------- LED / Ecran ---------- */
static void slider_cb(lv_event_t *e)
{
    intptr_t id = (intptr_t)lv_event_get_user_data(e);
    int v = lv_slider_get_value(lv_event_get_target_obj(e));
    if (id < 3) {
        s_rgb[id] = (uint8_t)v;
        board_led_set(s_rgb[0], s_rgb[1], s_rgb[2]);
    } else {
        board_backlight_set((uint8_t)v);
    }
}

static void add_slider(lv_obj_t *parent, const char *name, int id, int min, int max, int init)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, name);
    lv_obj_t *s = lv_slider_create(parent);
    lv_obj_set_width(s, lv_pct(85));
    lv_slider_set_range(s, min, max);
    lv_slider_set_value(s, init, LV_ANIM_OFF);
    lv_obj_add_event_cb(s, slider_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)id);
}

/* ---------- Lumiere ---------- */
static void ldr_timer_cb(lv_timer_t *t)
{
    (void)t;
    int p = board_ldr_read_percent();
    lv_bar_set_value(s_ldr_bar, p, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_ldr_label, "Lumiere ambiante : %d %%", p);
    if (lv_obj_has_state(s_auto_sw, LV_STATE_CHECKED)) board_backlight_set((uint8_t)(p < 10 ? 10 : p));
}

static lv_obj_t *add_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    return b;
}

void ui_create(void)
{
    lv_obj_t *tv = lv_tabview_create(lv_screen_active());
    lv_tabview_set_tab_bar_size(tv, 36);

    /* Info */
    lv_obj_t *t1 = lv_tabview_add_tab(tv, "Info");
    s_info_label = lv_label_create(t1);
    lv_label_set_text(s_info_label, "...");

    /* Scan WiFi */
    lv_obj_t *t2 = lv_tabview_add_tab(tv, "Scan");
    lv_obj_set_flex_flow(t2, LV_FLEX_FLOW_COLUMN);
    add_button(t2, "Scanner WiFi", wifi_btn_cb);
    s_wifi_label = lv_label_create(t2);
    lv_label_set_text(s_wifi_label, "Appuie sur Scanner");

    /* Net : connexion WiFi */
    lv_obj_t *t3 = lv_tabview_add_tab(tv, "Net");
    lv_obj_set_flex_flow(t3, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *l = lv_label_create(t3);
    lv_label_set_text(l, "Reseau (scan d'abord) :");
    s_dd = lv_dropdown_create(t3);
    lv_dropdown_set_options(s_dd, "(faire un scan)");
    lv_obj_set_width(s_dd, lv_pct(90));
    s_pass = lv_textarea_create(t3);
    lv_textarea_set_one_line(s_pass, true);
    lv_textarea_set_password_mode(s_pass, true);
    lv_textarea_set_placeholder_text(s_pass, "Mot de passe");
    lv_obj_set_width(s_pass, lv_pct(90));
    lv_obj_add_event_cb(s_pass, ta_cb, LV_EVENT_CLICKED, NULL);
    add_button(t3, "Connecter", connect_btn_cb);
    s_net_status = lv_label_create(t3);
    lv_label_set_text(s_net_status, "Non connecte");

    /* BLE */
    lv_obj_t *t4 = lv_tabview_add_tab(tv, "BLE");
    lv_obj_set_flex_flow(t4, LV_FLEX_FLOW_COLUMN);
    add_button(t4, "Scanner BLE", ble_btn_cb);
    s_ble_label = lv_label_create(t4);
    lv_label_set_text(s_ble_label, "Appuie sur Scanner");

    /* LED / Ecran */
    lv_obj_t *t5 = lv_tabview_add_tab(tv, "LED");
    lv_obj_set_flex_flow(t5, LV_FLEX_FLOW_COLUMN);
    add_slider(t5, "Rouge", 0, 0, 255, 0);
    add_slider(t5, "Vert", 1, 0, 255, 0);
    add_slider(t5, "Bleu", 2, 0, 255, 0);
    add_slider(t5, "Luminosite ecran", 3, 5, 100, 80);

    /* Lumiere */
    lv_obj_t *t6 = lv_tabview_add_tab(tv, "Lumi");
    lv_obj_set_flex_flow(t6, LV_FLEX_FLOW_COLUMN);
    s_ldr_label = lv_label_create(t6);
    lv_label_set_text(s_ldr_label, "...");
    s_ldr_bar = lv_bar_create(t6);
    lv_obj_set_width(s_ldr_bar, lv_pct(85));
    lv_bar_set_range(s_ldr_bar, 0, 100);
    l = lv_label_create(t6);
    lv_label_set_text(l, "Luminosite auto");
    s_auto_sw = lv_switch_create(t6);

    /* Clavier (cache tant qu'on ne touche pas le champ mot de passe) */
    s_kb = lv_keyboard_create(lv_screen_active());
    lv_obj_set_size(s_kb, lv_pct(100), 130);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_kb, kb_cb, LV_EVENT_ALL, NULL);

    lv_timer_create(info_timer_cb, 1000, NULL);
    lv_timer_create(ldr_timer_cb, 300, NULL);
    info_timer_cb(NULL);
}
