#pragma once
#include <stdint.h>
#include "esp_err.h"

/* ---- Ecran ILI9341 (SPI2) ---- */
#define LCD_H_RES        320      /* paysage */
#define LCD_V_RES        240
#define PIN_LCD_MOSI     13
#define PIN_LCD_MISO     12
#define PIN_LCD_SCLK     14
#define PIN_LCD_CS       15
#define PIN_LCD_DC       2
#define PIN_BL           21       /* retroeclairage */

/* ---- Tactile XPT2046 (SPI3) ---- */
#define PIN_TOUCH_CLK    25
#define PIN_TOUCH_MOSI   32
#define PIN_TOUCH_MISO   39
#define PIN_TOUCH_CS     33

/* ---- LED RGB (active a l'etat bas) et capteur de lumiere ---- */
#define PIN_LED_R        4
#define PIN_LED_G        16
#define PIN_LED_B        17
#define LDR_ADC_CHANNEL  ADC_CHANNEL_6   /* GPIO34 */

/* ---- A ajuster selon ton exemplaire de carte (voir README) ---- */
#define LCD_INVERT_COLORS 0
#define LCD_SWAP_XY       1
#define LCD_MIRROR_X      1
#define LCD_MIRROR_Y      0
#define TOUCH_SWAP_XY     1
#define TOUCH_MIRROR_X    0
#define TOUCH_MIRROR_Y    0
#define LDR_INVERT        0   /* 1 si "plus de lumiere = valeur plus basse" */

esp_err_t board_init(void);              /* PWM (backlight + LED) + ADC */
esp_err_t board_display_init(void);      /* ecran + tactile + LVGL */
void      board_backlight_set(uint8_t percent);
void      board_led_set(uint8_t r, uint8_t g, uint8_t b);
int       board_ldr_read_percent(void);  /* 0..100 */
