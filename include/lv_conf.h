/**
 * lv_conf.h — LVGL v9 configuration for BambuHelper firmware
 * Round 240×240 display (GC9A01A), ESP32-S3, 16-bit RGB565 color
 */
/* clang-format off */
#if 1  /* Set to 1 to enable content */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/** Color depth: 16 = RGB565 to match TFT_eSPI */
#define LV_COLOR_DEPTH 16

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

/*====================
   MEMORY SETTINGS
 *====================*/
/* Using CLIB malloc — no builtin pool needed */

/*=======================
   HAL SETTINGS
 *=======================*/
#define LV_DEF_REFR_PERIOD   20   /* [ms] default display refresh period */
#define LV_INDEV_DEF_READ_PERIOD 30

/*===================
   RENDERING BACKEND
 *===================*/
#define LV_USE_DRAW_SW 1
#define LV_USE_DRAW_SW_COMPLEX_GRADIENTS 0
#define LV_USE_NATIVE_HELIUM_ASM 0
#define LV_USE_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE
#define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED 1

/*=======================
   LOGGING
 *=======================*/
#define LV_USE_LOG 0

/*=======================
   ASSERTS
 *=======================*/
#define LV_USE_ASSERT_NULL          0
#define LV_USE_ASSERT_MALLOC        0
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/*=======================
   FONTS
 *=======================*/
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_40 1

#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*=======================
   WIDGETS
 *=======================*/
#define LV_USE_CALENDAR                  0
#define LV_USE_CALENDAR_HEADER_DROPDOWN  0
#define LV_USE_CALENDAR_HEADER_SIMPLE    0
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BUTTON     1   /* was LV_USE_BTN in v8 */
#define LV_USE_BTNMATRIX  0
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMAGE      1   /* was LV_USE_IMG in v8 */
#define LV_USE_LABEL      1
#define LV_USE_LINE       0
#define LV_USE_SCALE      0
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_MSGBOX     0
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1
#define LV_USE_SWITCH     0
#define LV_USE_KEYBOARD   0
#define LV_USE_TABVIEW    0
#define LV_USE_TABLE      0
#define LV_USE_TEXTAREA   0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/*=======================
   THEMES
 *=======================*/
#define LV_USE_THEME_DEFAULT  1
#define LV_USE_THEME_SIMPLE   0
#define LV_USE_THEME_MONO     0

/*=======================
   LAYOUTS
 *=======================*/
#define LV_USE_FLEX 1
#define LV_USE_GRID 0

/*=======================
   MISC
 *=======================*/
#define LV_USE_SNAPSHOT   0
#define LV_USE_FRAGMENT   0
#define LV_USE_IMGFONT    0

#endif /* LV_CONF_H */
#endif /* End "content enable" */
