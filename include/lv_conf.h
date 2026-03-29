/**
 * lv_conf.h — LVGL v8 configuration for BambuHelper
 * Round 240×240 display (GC9A01A), ESP32-S3, 16-bit color
 */
#if 1  /* Set to 1 to enable content */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Color depth: 16 (RGB565) to match TFT_eSPI */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 color.
   TFT_eSPI on ESP32 uses big-endian over SPI, LVGL produces little-endian. */
#define LV_COLOR_16_SWAP 1

/* Enable anti-aliasing */
#define LV_ANTIALIAS 1

/* Display resolution */
#define LV_HOR_RES_MAX 240
#define LV_VER_RES_MAX 240

/* DPI (not critical for this project) */
#define LV_DPI_DEF 130

/* ── Memory ────────────────────────────────────────────────────────────────── */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE   (48 * 1024U)   /* 48 KB heap for LVGL */
#define LV_MEM_ADR    0              /* 0 = use malloc */

/* ── HAL settings ──────────────────────────────────────────────────────────── */
#define LV_DISP_DEF_REFR_PERIOD  20   /* [ms] default display refresh period */
#define LV_INDEV_DEF_READ_PERIOD 30   /* [ms] input device read period */

/* ── Logging ───────────────────────────────────────────────────────────────── */
#define LV_USE_LOG 0

/* ── Asserts ───────────────────────────────────────────────────────────────── */
#define LV_USE_ASSERT_NULL          0
#define LV_USE_ASSERT_MALLOC        0
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/* ── Fonts ─────────────────────────────────────────────────────────────────── */
/* Montserrat fonts */
#define LV_FONT_MONTSERRAT_14 1   /* gauge labels, small text, launcher */
#define LV_FONT_MONTSERRAT_16 1   /* connecting status, idle status */
#define LV_FONT_MONTSERRAT_20 1   /* gauge values, ETA, clock date, AP SSID */
#define LV_FONT_MONTSERRAT_28 1   /* larger headings */
#define LV_FONT_MONTSERRAT_40 1   /* clock time digits */

/* Default font — used by widgets */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* ── Widgets ───────────────────────────────────────────────────────────────── */
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  0
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       0
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_METER      0
#define LV_USE_MSGBOX     0
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1   /* connecting screen animated arc */
#define LV_USE_SWITCH     0
#define LV_USE_TABVIEW    0
#define LV_USE_TABLE      0
#define LV_USE_TEXTAREA   0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/* ── Themes ────────────────────────────────────────────────────────────────── */
#define LV_USE_THEME_DEFAULT  1
#define LV_USE_THEME_BASIC    0
#define LV_USE_THEME_MONO     0

/* ── Layouts ───────────────────────────────────────────────────────────────── */
#define LV_USE_FLEX  1
#define LV_USE_GRID  0

/* ── Extra widgets (require disabled base widgets — must be off) ────────────── */
#define LV_USE_ANIMIMG        0   /* requires LV_USE_IMG */
#define LV_USE_CALENDAR       0
#define LV_USE_CHART          0
#define LV_USE_COLORWHEEL     0
#define LV_USE_IMGBTN         0   /* requires LV_USE_IMG */
#define LV_USE_KEYBOARD       0   /* requires LV_USE_BTNMATRIX + LV_USE_TEXTAREA */
#define LV_USE_LED            0
#define LV_USE_LIST           0
#define LV_USE_METER          0
#define LV_USE_MSGBOX         0
#define LV_USE_SPINBOX        0
#define LV_USE_TABVIEW        0
#define LV_USE_TILEVIEW       0
#define LV_USE_WIN            0

/* ── Misc ──────────────────────────────────────────────────────────────────── */
#define LV_USE_SNAPSHOT       0
#define LV_USE_MONKEY         0
#define LV_USE_GRIDNAV        0
#define LV_USE_FRAGMENT       0
#define LV_USE_IMGFONT        0
#define LV_USE_MSG            0
#define LV_USE_IME_PINYIN     0

/* Animation: keep enabled for smooth button press feedback */
#define LV_USE_ANIMATION 1

/* Draw masks — required for arc widgets, rounded corners, and circular shapes.
   Without this, lv_arc may not render correctly. */
#define LV_DRAW_COMPLEX 1

/* GC9A01A is a round display — hardware clips pixels outside the circle.
   LVGL treats it as a standard 240x240 rect; no circular masking needed in software. */

#endif /* LV_CONF_H */
#endif /* End "content enable" */
