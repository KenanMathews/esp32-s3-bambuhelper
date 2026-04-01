#include "lvgl_port.h"
#include "button.h"       // for getTouchXY()
#include "settings.h"     // for dispSettings.rotation
#include <TFT_eSPI.h>

// TFT_eSPI instance is owned by display_ui.cpp; only used here for SPI flushing.
extern TFT_eSPI tft;
#include <lvgl.h>

// ---------------------------------------------------------------------------
//  Display flush — LVGL 9 calls this to push a rendered region to the screen
// ---------------------------------------------------------------------------
static lv_display_t* g_disp = nullptr;

// Two 20-line buffers: LVGL can render one while the other is being flushed
static lv_color_t lv_buf1[240 * 20];
static lv_color_t lv_buf2[240 * 20];

static void disp_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t*)px_map, w * h, false);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

// ---------------------------------------------------------------------------
//  Touch read — LVGL 9 input device callback
// ---------------------------------------------------------------------------
static lv_indev_t* g_indev = nullptr;

static void touch_read(lv_indev_t* indev, lv_indev_data_t* data) {
  int16_t x = 0, y = 0;
  if (getTouchXY(&x, &y)) {
    int16_t tx = x, ty = y;
    switch (dispSettings.rotation) {
      case 1: tx = y;       ty = 239 - x; break;
      case 2: tx = 239 - x; ty = 239 - y; break;
      case 3: tx = 239 - y; ty = x;       break;
      default: break;
    }
    data->state   = LV_INDEV_STATE_PRESSED;
    data->point.x = tx;
    data->point.y = ty;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ---------------------------------------------------------------------------
//  lvglPortInit
// ---------------------------------------------------------------------------
void lvglPortInit() {
  lv_init();

  // Create display — LVGL 9 API
  g_disp = lv_display_create(240, 240);
  lv_display_set_flush_cb(g_disp, disp_flush);
  lv_display_set_buffers(g_disp, lv_buf1, lv_buf2, sizeof(lv_buf1),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_color_format(g_disp, LV_COLOR_FORMAT_RGB565_SWAPPED);

  // Theme
  lv_theme_t* th = lv_theme_default_init(
      g_disp,
      lv_palette_main(LV_PALETTE_GREEN),
      lv_palette_main(LV_PALETTE_LIGHT_GREEN),
      true,
      &lv_font_montserrat_14);
  lv_display_set_theme(g_disp, th);

  // Create touch input device — LVGL 9 API
  g_indev = lv_indev_create();
  lv_indev_set_type(g_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(g_indev, touch_read);
}

// ---------------------------------------------------------------------------
//  lvglPortTick — call every loop; drives LVGL's internal timer
// ---------------------------------------------------------------------------
void lvglPortTick() {
  lv_timer_handler();
  lv_tick_inc(5);
}
