#include "lvgl_port.h"
#include "display_ui.h"   // for extern TFT_eSPI tft
#include "button.h"       // for getTouchXY()
#include <lvgl.h>

// ---------------------------------------------------------------------------
//  Display flush — LVGL calls this to push a rendered region to the screen
// ---------------------------------------------------------------------------
static lv_disp_draw_buf_t draw_buf;
// Two 20-line buffers: LVGL can render one while the other is being flushed
static lv_color_t lv_buf1[240 * 20];
static lv_color_t lv_buf2[240 * 20];

static void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t*)color_p, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(drv);
}

// ---------------------------------------------------------------------------
//  Touch read — LVGL calls this to poll the input device
// ---------------------------------------------------------------------------
static void touch_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  int16_t x = 0, y = 0;
  if (getTouchXY(&x, &y)) {
    data->state   = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ---------------------------------------------------------------------------
//  lvglPortInit
// ---------------------------------------------------------------------------
void lvglPortInit() {
  lv_init();

  // Double-buffer: smoother rendering at the cost of 2 × 240 × 20 × 2 = ~19 KB
  lv_disp_draw_buf_init(&draw_buf, lv_buf1, lv_buf2, 240 * 20);

  // Register display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = 240;
  disp_drv.ver_res  = 240;
  disp_drv.flush_cb = disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Register touch input driver
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touch_read;
  lv_indev_drv_register(&indev_drv);
}

// ---------------------------------------------------------------------------
//  lvglPortTick — call every loop; drives LVGL's internal timer
// ---------------------------------------------------------------------------
void lvglPortTick() {
  lv_timer_handler();
  lv_tick_inc(5);   // tell LVGL ~5 ms has passed (loop runs faster than this)
}
