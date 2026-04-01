#include "display_launcher.h"
#include "app_manager.h"
#include "lvgl_port.h"
#include "config.h"
#include <Arduino.h>
#include <lvgl.h>
#include <time.h>

// ---------------------------------------------------------------------------
//  Color helper — same approach as other screens (avoids lv_color_hex issue)
// ---------------------------------------------------------------------------
static inline lv_color_t c565(uint16_t rgb565) {
    uint8_t r = ((rgb565 >> 11) & 0x1F) * 8;
    uint8_t g = ((rgb565 >> 5)  & 0x3F) * 4;
    uint8_t b = (rgb565 & 0x1F) * 8;
    return lv_color_make(r, g, b);
}

// ---------------------------------------------------------------------------
//  Layout constants
// ---------------------------------------------------------------------------
static const unsigned long AUTO_DISMISS_MS = 5000;

// Tile size
#define TILE_W   68
#define TILE_H   68
#define TILE_R   10   // corner radius

// x positions for columns 0,1,2
static const int16_t COL_X[3] = { 9, 86, 163 };
// y positions for rows 0,1
static const int16_t ROW_Y[2] = { 34, 136 };

// ---------------------------------------------------------------------------
//  State
// ---------------------------------------------------------------------------
static lv_obj_t*     g_screen   = NULL;
static lv_obj_t*     g_tiles[LAUNCHER_GRID_SIZE];
static lv_obj_t*     g_clock_lbl = NULL;   // symbol label on the clock tile
static int8_t        g_selected  = -1;
static unsigned long g_enterMs   = 0;
static unsigned long g_lastClockUpdate = 0;

// ---------------------------------------------------------------------------
//  Tile event callback
// ---------------------------------------------------------------------------
static void tile_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_obj_t* tile = (lv_obj_t*)lv_event_get_target(e);
    for (uint8_t i = 0; i < LAUNCHER_GRID_SIZE; i++) {
        if (g_tiles[i] == tile) { g_selected = (int8_t)i; break; }
    }
}

// ---------------------------------------------------------------------------
//  buildTile — creates one 68×68 tile
//  isClockTile: shows live HH:MM as symbol text instead of a fixed icon
// ---------------------------------------------------------------------------
static void buildTile(uint8_t idx, int16_t x, int16_t y,
                      const char* symbol, const char* label,
                      uint16_t color, bool isClockTile) {
    lv_obj_t* tile = lv_btn_create(g_screen);
    g_tiles[idx] = tile;

    lv_obj_set_pos(tile, x, y);
    lv_obj_set_size(tile, TILE_W, TILE_H);

    lv_obj_set_style_bg_color(tile, c565(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(tile, c565(CLR_BTN_PR), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(tile, TILE_R, LV_PART_MAIN);
    lv_obj_set_style_border_width(tile, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(tile, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tile, 0, LV_PART_MAIN);

    lv_obj_add_event_cb(tile, tile_event_cb, LV_EVENT_CLICKED, NULL);

    // Symbol / time label (center, offset y=-10)
    lv_obj_t* sym_lbl = lv_label_create(tile);
    if (isClockTile) {
        // Initial HH:MM text
        time_t t = time(nullptr);
        struct tm tm_info;
        localtime_r(&t, &tm_info);
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
        lv_label_set_text(sym_lbl, buf);
        lv_obj_set_style_text_font(sym_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
        g_clock_lbl = sym_lbl;
    } else {
        lv_label_set_text(sym_lbl, symbol);
        lv_obj_set_style_text_font(sym_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    }
    lv_obj_set_style_text_color(sym_lbl, c565(CLR_TEXT), LV_PART_MAIN);
    lv_obj_align(sym_lbl, LV_ALIGN_CENTER, 0, -10);

    // Name label (center, offset y=+14)
    lv_obj_t* name_lbl = lv_label_create(tile);
    lv_label_set_text(name_lbl, label);
    lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(name_lbl, c565(CLR_TEXT), LV_PART_MAIN);
    lv_obj_align(name_lbl, LV_ALIGN_CENTER, 0, 14);
}

// ---------------------------------------------------------------------------
//  buildScreen
// ---------------------------------------------------------------------------
static void buildScreen() {
    if (g_screen == NULL) {
        g_screen = lv_obj_create(NULL);
    } else {
        lv_obj_clean(g_screen);
        g_clock_lbl = NULL;
    }

    lv_obj_set_style_bg_color(g_screen, c565(CLR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_screen, 0, LV_PART_MAIN);

    // --- Built-in tiles ---
    // 0: Dashboard
    buildTile(0, COL_X[0], ROW_Y[0], LV_SYMBOL_HOME,     "Dash",  CLR_GREEN,  false);
    // 1: Clock (live time)
    buildTile(1, COL_X[1], ROW_Y[0], "",                 "Clock", CLR_BLUE,   true);
    // 2: Store
    buildTile(2, COL_X[2], ROW_Y[0], LV_SYMBOL_DOWNLOAD, "Store", 0x0410,     false);
    // 3: Settings
    buildTile(3, COL_X[0], ROW_Y[1], LV_SYMBOL_SETTINGS, "Setup", CLR_ORANGE, false);

    // --- User app slots (4 and 5) ---
    for (uint8_t slot = 0; slot < 2; slot++) {
        uint8_t idx = LAUNCHER_USER_BASE + slot;
        uint8_t col = idx % LAUNCHER_GRID_COLS;
        uint8_t row = idx / LAUNCHER_GRID_COLS;
        int16_t x   = COL_X[col];
        int16_t y   = ROW_Y[row];

        uint8_t appCount = appManagerCount();
        if (slot < appCount) {
            const AppInfo* app = appManagerGetApp(slot);
            buildTile(idx, x, y, LV_SYMBOL_PLAY, app->name, app->color, false);
        } else {
            buildTile(idx, x, y, LV_SYMBOL_PLUS, "App", CLR_BTN, false);
        }
    }
}

// ---------------------------------------------------------------------------
//  launcherEnter
// ---------------------------------------------------------------------------
void launcherEnter() {
    g_enterMs         = millis();
    g_selected        = -1;
    g_lastClockUpdate = 0;

    buildScreen();
    lv_scr_load(g_screen);
    lvglPortTick();
}

// ---------------------------------------------------------------------------
//  launcherUpdate
// ---------------------------------------------------------------------------
int8_t launcherUpdate() {
    lvglPortTick();

    // Update clock tile every second
    if (g_clock_lbl != NULL) {
        unsigned long now = millis();
        if (now - g_lastClockUpdate >= 1000) {
            g_lastClockUpdate = now;
            time_t t = time(nullptr);
            struct tm tm_info;
            localtime_r(&t, &tm_info);
            char buf[6];
            snprintf(buf, sizeof(buf), "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
            lv_label_set_text(g_clock_lbl, buf);
        }
    }

    if (millis() - g_enterMs > AUTO_DISMISS_MS) return -2;

    if (g_selected >= 0) {
        int8_t result = g_selected;
        g_selected = -1;
        return result;
    }

    return -1;
}
