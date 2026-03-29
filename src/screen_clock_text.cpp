/*
 * screen_clock_text.cpp — LVGL digital clock screen
 *
 * Replaces the TFT_eSPI-based clock_mode.cpp for the SCREEN_CLOCK state
 * when pongClock is disabled.
 *
 * Layout (240×240):
 *   y≈82  — time digits (large, lv_font_montserrat_40)
 *   y≈120 — AM/PM if 12h mode (medium)
 *   y≈148 — date string (medium)
 */

#include "screen_clock_text.h"
#include "config.h"
#include "settings.h"
#include <lvgl.h>
#include <time.h>

static lv_obj_t* g_scr        = nullptr;
static lv_obj_t* g_lbl_time   = nullptr;
static lv_obj_t* g_lbl_ampm   = nullptr;
static lv_obj_t* g_lbl_date   = nullptr;

static int g_prevMinute = -1;

static inline lv_color_t c565(uint16_t rgb565) {
    uint8_t r = ((rgb565 >> 11) & 0x1F) * 8;
    uint8_t g = ((rgb565 >> 5)  & 0x3F) * 4;
    uint8_t b = (rgb565 & 0x1F) * 8;
    return lv_color_make(r, g, b);
}

void clockTextScreenInit() {
    g_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_scr, c565(dispSettings.bgColor), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_scr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_scr, 0, LV_PART_MAIN);

    // Large time digits
    g_lbl_time = lv_label_create(g_scr);
    lv_label_set_text(g_lbl_time, "--:--");
    lv_obj_set_style_text_font(g_lbl_time, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lbl_time, c565(CLR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_lbl_time, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(g_lbl_time, LV_ALIGN_CENTER, 0, -30);

    // AM/PM (hidden in 24h mode)
    g_lbl_ampm = lv_label_create(g_scr);
    lv_label_set_text(g_lbl_ampm, "");
    lv_obj_set_style_text_font(g_lbl_ampm, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lbl_ampm, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_lbl_ampm, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(g_lbl_ampm, LV_ALIGN_CENTER, 0, 16);

    // Date string
    g_lbl_date = lv_label_create(g_scr);
    lv_label_set_text(g_lbl_date, "");
    lv_obj_set_style_text_font(g_lbl_date, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lbl_date, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_lbl_date, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(g_lbl_date, LV_ALIGN_CENTER, 0, 38);
}

lv_obj_t* clockTextScreenGet() {
    return g_scr;
}

void clockTextScreenReset() {
    g_prevMinute = -1;
}

void clockTextScreenUpdate() {
    struct tm now;
    if (!getLocalTime(&now, 0)) return;

    if (now.tm_min == g_prevMinute) return;
    g_prevMinute = now.tm_min;

    // Time string
    char timeBuf[12];
    if (netSettings.use24h) {
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", now.tm_hour, now.tm_min);
        lv_label_set_text(g_lbl_ampm, "");
    } else {
        int h = now.tm_hour % 12;
        if (h == 0) h = 12;
        snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d", h, now.tm_min);
        lv_label_set_text(g_lbl_ampm, now.tm_hour < 12 ? "AM" : "PM");
    }
    lv_label_set_text(g_lbl_time, timeBuf);

    // Date string — locale-aware: DD.MM.YYYY in 24h mode, MM/DD/YYYY in 12h
    static const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    char dateBuf[24];
    if (netSettings.use24h)
        snprintf(dateBuf, sizeof(dateBuf), "%s  %02d.%02d.%04d",
                 days[now.tm_wday],
                 now.tm_mday, now.tm_mon + 1, now.tm_year + 1900);
    else
        snprintf(dateBuf, sizeof(dateBuf), "%s  %02d/%02d/%04d",
                 days[now.tm_wday],
                 now.tm_mon + 1, now.tm_mday, now.tm_year + 1900);
    lv_label_set_text(g_lbl_date, dateBuf);
}
