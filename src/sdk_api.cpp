#include "sdk_api.h"
#include "config.h"
#include <Arduino.h>

#ifdef LUA_AVAILABLE

#include "bambu_state.h"
#include "buzzer.h"
#include <lvgl.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

// ---------------------------------------------------------------------------
//  sys.exit() flag — lua_runtime.cpp checks this after pcall
// ---------------------------------------------------------------------------
static bool g_exit_requested = false;

bool sdkExitRequested() { return g_exit_requested; }
void sdkClearExitFlag()  { g_exit_requested = false; }

// ---------------------------------------------------------------------------
//  Color helper
// ---------------------------------------------------------------------------
static inline lv_color_t c565(uint16_t rgb565) {
    uint8_t r = ((rgb565 >> 11) & 0x1F) * 8;
    uint8_t g = ((rgb565 >> 5)  & 0x3F) * 4;
    uint8_t b = (rgb565 & 0x1F) * 8;
    return lv_color_make(r, g, b);
}

// ---------------------------------------------------------------------------
//  Font lookup by size
// ---------------------------------------------------------------------------
static const lv_font_t* font_by_size(int sz) {
    if (sz <= 14) return &lv_font_montserrat_14;
    if (sz <= 16) return &lv_font_montserrat_16;
    if (sz <= 20) return &lv_font_montserrat_20;
    return &lv_font_montserrat_28;
}

// ---------------------------------------------------------------------------
//  Alignment string → lv_align_t
// ---------------------------------------------------------------------------
static lv_align_t parse_align(const char* s) {
    if (!s) return LV_ALIGN_DEFAULT;
    if (strcmp(s, "center")     == 0) return LV_ALIGN_CENTER;
    if (strcmp(s, "top_mid")    == 0) return LV_ALIGN_TOP_MID;
    if (strcmp(s, "top_left")   == 0) return LV_ALIGN_TOP_LEFT;
    if (strcmp(s, "top_right")  == 0) return LV_ALIGN_TOP_RIGHT;
    if (strcmp(s, "bottom_mid") == 0) return LV_ALIGN_BOTTOM_MID;
    if (strcmp(s, "bottom_left")== 0) return LV_ALIGN_BOTTOM_LEFT;
    if (strcmp(s, "bottom_right")== 0)return LV_ALIGN_BOTTOM_RIGHT;
    if (strcmp(s, "left_mid")   == 0) return LV_ALIGN_LEFT_MID;
    if (strcmp(s, "right_mid")  == 0) return LV_ALIGN_RIGHT_MID;
    return LV_ALIGN_DEFAULT;
}

// ---------------------------------------------------------------------------
//  Helper: get typed field from Lua table (use native types to avoid
//  extern-"C" typedef visibility issues with lua_Integer / lua_Number)
// ---------------------------------------------------------------------------
static long long tbl_int(lua_State* L, int idx, const char* key, long long def) {
    lua_getfield(L, idx, key);
    long long v = lua_isnil(L, -1) ? def : (long long)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return v;
}

static double tbl_num(lua_State* L, int idx, const char* key, double def) {
    lua_getfield(L, idx, key);
    double v = lua_isnil(L, -1) ? def : (double)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return v;
}

static const char* tbl_str(lua_State* L, int idx, const char* key, const char* def) {
    lua_getfield(L, idx, key);
    const char* v = lua_isnil(L, -1) ? def : lua_tostring(L, -1);
    lua_pop(L, 1);
    return v;
}

// ===========================================================================
//  bambu.* module
// ===========================================================================

static int bambu_state(lua_State* L) {
    lua_pushstring(L, displayedPrinter().state.gcodeState);
    return 1;
}

static int bambu_progress(lua_State* L) {
    lua_pushinteger(L, (long long)displayedPrinter().state.progress);
    return 1;
}

static int bambu_nozzle_temp(lua_State* L) {
    lua_pushnumber(L, (lua_Number)displayedPrinter().state.nozzleTemp);
    return 1;
}

static int bambu_bed_temp(lua_State* L) {
    lua_pushnumber(L, (lua_Number)displayedPrinter().state.bedTemp);
    return 1;
}

static int bambu_nozzle_target(lua_State* L) {
    lua_pushnumber(L, (lua_Number)displayedPrinter().state.nozzleTarget);
    return 1;
}

static int bambu_bed_target(lua_State* L) {
    lua_pushnumber(L, (lua_Number)displayedPrinter().state.bedTarget);
    return 1;
}

static int bambu_remaining_mins(lua_State* L) {
    lua_pushinteger(L, (long long)displayedPrinter().state.remainingMinutes);
    return 1;
}

static int bambu_fan_part(lua_State* L) {
    lua_pushinteger(L, (long long)displayedPrinter().state.coolingFanPct);
    return 1;
}

static int bambu_fan_aux(lua_State* L) {
    lua_pushinteger(L, (long long)displayedPrinter().state.auxFanPct);
    return 1;
}

static int bambu_printer_name(lua_State* L) {
    lua_pushstring(L, displayedPrinter().config.name);
    return 1;
}

static int bambu_connected(lua_State* L) {
    lua_pushboolean(L, displayedPrinter().state.connected ? 1 : 0);
    return 1;
}

static int bambu_printing(lua_State* L) {
    lua_pushboolean(L, displayedPrinter().state.printing ? 1 : 0);
    return 1;
}

static const luaL_Reg bambu_lib[] = {
    { "state",          bambu_state          },
    { "progress",       bambu_progress       },
    { "nozzle_temp",    bambu_nozzle_temp    },
    { "bed_temp",       bambu_bed_temp       },
    { "nozzle_target",  bambu_nozzle_target  },
    { "bed_target",     bambu_bed_target     },
    { "remaining_mins", bambu_remaining_mins },
    { "fan_part",       bambu_fan_part       },
    { "fan_aux",        bambu_fan_aux        },
    { "printer_name",   bambu_printer_name   },
    { "connected",      bambu_connected      },
    { "printing",       bambu_printing       },
    { nullptr,          nullptr              }
};

// ===========================================================================
//  sys.* module
// ===========================================================================

static int sys_millis(lua_State* L) {
    unsigned long ms = millis();
    lua_pushinteger(L, (long long)ms);
    return 1;
}

static int sys_beep(lua_State* L) {
    (void)luaL_optnumber(L, 1, 1000.0);
    (void)luaL_optnumber(L, 2, 100.0);
    buzzerPlay(BUZZ_CONNECTED);
    return 0;
}

static int sys_log(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    Serial.println(msg);
    return 0;
}

static int sys_exit(lua_State* L) {
    (void)L;
    g_exit_requested = true;
    return 0;
}

static int sys_sdk_version(lua_State* L) {
    lua_pushinteger(L, SDK_VERSION);
    return 1;
}

static const luaL_Reg sys_lib[] = {
    { "millis",      sys_millis      },
    { "beep",        sys_beep        },
    { "log",         sys_log         },
    { "exit",        sys_exit        },
    { "sdk_version", sys_sdk_version },
    { nullptr,       nullptr         }
};

// ===========================================================================
//  ui.* module — LVGL 8 implementation
//
//  ui.screen()                   → lightuserdata (lv_obj_t*)
//  ui.label(scr, text, opts)     → nil
//  ui.arc(scr, opts)             → nil
//  ui.rect(scr, opts)            → nil
//  ui.show(scr)                  → nil
//
//  opts table keys:
//    align  string  "center", "top_mid", etc.
//    x, y   int     offset from align anchor
//    color  int     RGB565
//    font   int     point size (14/16/20/28)
//    cx,cy  int     center pixel (arc/rect)
//    size   int     arc radius
//    value  int     arc 0-100 percent
//    w, h   int     rect width/height
// ===========================================================================

static int ui_screen(lua_State* L) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, c565(CLR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lua_pushlightuserdata(L, scr);
    return 1;
}

static int ui_label(lua_State* L) {
    lv_obj_t* parent = (lv_obj_t*)lua_touserdata(L, 1);
    const char* text = luaL_checkstring(L, 2);
    if (!parent) return 0;

    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);

    if (lua_istable(L, 3)) {
        uint16_t color = (uint16_t)tbl_int(L, 3, "color", CLR_TEXT);
        int      font  = (int)     tbl_int(L, 3, "font",  16);
        const char* align_s = tbl_str(L, 3, "align", nullptr);
        int      ox    = (int)     tbl_int(L, 3, "x", 0);
        int      oy    = (int)     tbl_int(L, 3, "y", 0);

        lv_obj_set_style_text_color(lbl, c565(color), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, font_by_size(font), LV_PART_MAIN);

        if (align_s) {
            lv_obj_align(lbl, parse_align(align_s), ox, oy);
        } else {
            lv_obj_set_pos(lbl, ox, oy);
        }
    }
    return 0;
}

static int ui_arc(lua_State* L) {
    lv_obj_t* parent = (lv_obj_t*)lua_touserdata(L, 1);
    if (!parent || !lua_istable(L, 2)) return 0;

    int     value  = (int)tbl_int(L, 2, "value", 0);
    int     size   = (int)tbl_int(L, 2, "size",  80);
    uint16_t color = (uint16_t)tbl_int(L, 2, "color", CLR_GREEN);
    int     cx     = (int)tbl_int(L, 2, "cx", 120);
    int     cy     = (int)tbl_int(L, 2, "cy", 120);

    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size * 2, size * 2);
    // Horseshoe: 135° to 45° (going clockwise, 270° sweep)
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_arc_set_angles(arc, 135, 135 + (int)(270 * value / 100));
    lv_arc_set_value(arc, value);

    lv_obj_set_style_arc_color(arc, c565(color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, c565(CLR_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_pos(arc, cx - size, cy - size);
    return 0;
}

static int ui_rect(lua_State* L) {
    lv_obj_t* parent = (lv_obj_t*)lua_touserdata(L, 1);
    if (!parent || !lua_istable(L, 2)) return 0;

    int      cx    = (int)    tbl_int(L, 2, "cx",    120);
    int      cy    = (int)    tbl_int(L, 2, "cy",    120);
    int      w     = (int)    tbl_int(L, 2, "w",      40);
    int      h     = (int)    tbl_int(L, 2, "h",      40);
    uint16_t color = (uint16_t)tbl_int(L, 2, "color", CLR_BTN);
    int      r     = (int)    tbl_int(L, 2, "radius",  0);

    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, cx - w / 2, cy - h / 2);
    lv_obj_set_style_bg_color(obj, c565(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, r, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    return 0;
}

static int ui_show(lua_State* L) {
    lv_obj_t* scr = (lv_obj_t*)lua_touserdata(L, 1);
    if (scr) lv_scr_load(scr);
    return 0;
}

static const luaL_Reg ui_lib[] = {
    { "screen", ui_screen },
    { "label",  ui_label  },
    { "arc",    ui_arc    },
    { "rect",   ui_rect   },
    { "show",   ui_show   },
    { nullptr,  nullptr   }
};

// ===========================================================================
//  Registration
// ===========================================================================

void sdkApiRegister(lua_State* L) {
    g_exit_requested = false;

    luaL_newlib(L, bambu_lib);
    lua_setglobal(L, "bambu");

    luaL_newlib(L, sys_lib);
    lua_setglobal(L, "sys");

    luaL_newlib(L, ui_lib);
    lua_setglobal(L, "ui");
}

#endif // LUA_AVAILABLE
