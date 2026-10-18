// Minimale LVGL 8.3-configuratie; alles wat hier niet staat gebruikt de standaardwaarde.
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

// Gebruik malloc (grote blokken gaan automatisch naar PSRAM)
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DISP_DEF_REFR_PERIOD 20
#define LV_INDEV_DEF_READ_PERIOD 20

// Eigen Montserrat-lettertypen (src/fonts) met West-Europese letters voor Engels, Frans, Duits en Spaans.
// De ingebouwde van LVGL hebben alleen ASCII. De oude namen blijven werken via de defines hieronder.
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(bm_font_14) LV_FONT_DECLARE(bm_font_16) LV_FONT_DECLARE(bm_font_20) LV_FONT_DECLARE(bm_font_28)
#define lv_font_montserrat_14 bm_font_14
#define lv_font_montserrat_16 bm_font_16
#define lv_font_montserrat_20 bm_font_20
#define lv_font_montserrat_28 bm_font_28
#define LV_FONT_DEFAULT &bm_font_16

// Gebruik snprintf uit de C-bibliotheek, zodat %.1f e.d. werken in lv_label_set_text_fmt
#define LV_SPRINTF_CUSTOM 1
#define LV_SPRINTF_INCLUDE <stdio.h>
#define lv_snprintf snprintf
#define lv_vsnprintf vsnprintf

// QR-code op de wifi-pagina: telefooncamera erop en hij verbindt met het wifipunt
#define LV_USE_QRCODE 1

#define LV_USE_LOG 0
#define LV_BUILD_EXAMPLES 0

#endif
