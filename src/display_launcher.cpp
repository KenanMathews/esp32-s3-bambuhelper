#include "display_launcher.h"
#include "lvgl_port.h"
#include "bambu_mqtt.h"
#include <lvgl.h>

// ---------------------------------------------------------------------------
//  Color palette (hex RGB888 → lv_color_hex)
// ---------------------------------------------------------------------------
#define LC_BG       0x0D0D1A   // near-black background
#define LC_BTN      0x1E3A8A   // blue button
#define LC_BTN_PR   0x2563EB   // blue pressed
#define LC_TEXT     0xE0E0FF   // primary text
#define LC_DIM      0x6666AA   // dimmed text / title
#define LC_DISABLED 0x333355   // disabled button bg

static const unsigned long AUTO_DISMISS_MS = 5000;

static const char* const ITEM_LABELS[LAUNCHER_ITEM_COUNT] = {
  "Dashboard",
  "Clock",
  "Settings",
  "Printer"
};

// ---------------------------------------------------------------------------
//  State
// ---------------------------------------------------------------------------
static lv_obj_t*     g_screen   = NULL;   // created once, reused every open
static lv_obj_t*     g_btns[LAUNCHER_ITEM_COUNT];
static int8_t        g_selected = -1;
static unsigned long g_enterMs  = 0;

// ---------------------------------------------------------------------------
//  Button event callback
// ---------------------------------------------------------------------------
static void btn_event_cb(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_obj_t* btn = lv_event_get_target(e);
  for (uint8_t i = 0; i < LAUNCHER_ITEM_COUNT; i++) {
    if (g_btns[i] == btn) { g_selected = (int8_t)i; break; }
  }
}

// ---------------------------------------------------------------------------
//  Build (or rebuild) the screen contents
// ---------------------------------------------------------------------------
static void buildScreen() {
  // Clear any previous children — screen itself is kept alive so LVGL never
  // tries to render a deleted active screen.
  if (g_screen == NULL) {
    g_screen = lv_obj_create(NULL);
  } else {
    lv_obj_clean(g_screen);
  }

  lv_obj_set_style_bg_color(g_screen, lv_color_hex(LC_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_screen, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_screen, 0, LV_PART_MAIN);

  // Title
  lv_obj_t* title = lv_label_create(g_screen);
  lv_label_set_text(title, "MENU");
  lv_obj_set_style_text_color(title, lv_color_hex(LC_DIM), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

  // Four menu buttons
  for (uint8_t i = 0; i < LAUNCHER_ITEM_COUNT; i++) {
    bool disabled = (i == LAUNCHER_PRINTER && getActiveConnCount() < 2);

    lv_obj_t* btn = lv_btn_create(g_screen);
    g_btns[i] = btn;
    lv_obj_set_size(btn, 160, 34);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 40 + i * 42);

    uint32_t bg  = disabled ? LC_DISABLED : LC_BTN;
    uint32_t bgp = disabled ? LC_DISABLED : LC_BTN_PR;
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg),  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bgp), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, ITEM_LABELS[i]);
    lv_obj_set_style_text_color(lbl, lv_color_hex(disabled ? LC_DIM : LC_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(lbl);

    if (!disabled) {
      lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
    } else {
      lv_obj_add_state(btn, LV_STATE_DISABLED);
    }
  }
}

// ---------------------------------------------------------------------------
//  launcherEnter
// ---------------------------------------------------------------------------
void launcherEnter() {
  g_enterMs  = millis();
  g_selected = -1;

  buildScreen();
  lv_scr_load(g_screen);

  // Run one tick so LVGL renders the screen before we return
  lvglPortTick();
}

// ---------------------------------------------------------------------------
//  launcherUpdate — drive LVGL; return selection or status code
// ---------------------------------------------------------------------------
int8_t launcherUpdate() {
  lvglPortTick();

  if (millis() - g_enterMs > AUTO_DISMISS_MS) return -2;

  if (g_selected >= 0) {
    int8_t result = g_selected;
    g_selected = -1;
    return result;
  }

  return -1;
}
