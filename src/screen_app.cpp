#include "screen_app.h"
#include "lua_runtime.h"
#include "config.h"
#include <lvgl.h>

static inline lv_color_t c565(uint16_t rgb565) {
    uint8_t r = ((rgb565 >> 11) & 0x1F) * 8;
    uint8_t g = ((rgb565 >> 5)  & 0x3F) * 4;
    uint8_t b = (rgb565 & 0x1F) * 8;
    return lv_color_make(r, g, b);
}

static lv_obj_t* g_screen     = nullptr;
static lv_obj_t* g_title_lbl  = nullptr;  // app name at top
static lv_obj_t* g_status_lbl = nullptr;  // center status text

void appScreenInit() {
    if (g_screen) lv_obj_del(g_screen);

    g_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_screen, c565(CLR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_screen, 0, LV_PART_MAIN);

    // Small dim app name label at top-center
    g_title_lbl = lv_label_create(g_screen);
    lv_label_set_text(g_title_lbl, "");
    lv_obj_set_style_text_font(g_title_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_title_lbl, c565(CLR_TEXT_DARK), LV_PART_MAIN);
    lv_obj_align(g_title_lbl, LV_ALIGN_TOP_MID, 0, 12);

    // Center status label
    g_status_lbl = lv_label_create(g_screen);
    lv_label_set_text(g_status_lbl, "");
    lv_obj_set_style_text_font(g_status_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_status_lbl, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    lv_obj_align(g_status_lbl, LV_ALIGN_CENTER, 0, 0);
}

lv_obj_t* appScreenGet() {
    return g_screen;
}

void appScreenPrepare(const char* appName) {
    if (g_title_lbl)  lv_label_set_text(g_title_lbl,  appName ? appName : "");
    if (g_status_lbl) lv_label_set_text(g_status_lbl, "Running...");
}

void appScreenUpdate() {
    if (!g_status_lbl) return;

    // If Lua script is no longer running, update status
    if (!luaRuntimeRunning()) {
        const char* err = luaRuntimeLastError();
        if (err && err[0] != '\0') {
            lv_label_set_text(g_status_lbl, "Error");
        } else {
            lv_label_set_text(g_status_lbl, "Done");
        }
    }
}
