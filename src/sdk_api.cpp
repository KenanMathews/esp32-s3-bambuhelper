#include "sdk_api.h"
#include "config.h"
#include <Arduino.h>

#ifdef LUA_AVAILABLE

#include "bambu_state.h"
#include "buzzer.h"
#include "settings.h"
#include "ble_manager.h"
#include <lvgl.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>
#include <Wire.h>
#include <SensorQMI8658.hpp>   // lewisxhe/SensorLib
#include <NimBLEDevice.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

// ---------------------------------------------------------------------------
//  Internal flags / refs
// ---------------------------------------------------------------------------
static bool       g_exit_requested = false;
static int        g_tick_ref       = LUA_NOREF;  // set by sys.on_tick()
static lua_State* g_L              = nullptr;    // cached for cleanup

// Memory tracking — cleaned up on app exit
#define MAX_CANVAS_BUFS 8
static void*      g_canvas_bufs[MAX_CANVAS_BUFS] = {};
static int        g_canvas_buf_count = 0;
static lv_obj_t*  g_lua_screen       = nullptr;  // first screen created by app

// sys.every() interval timers
#define MAX_EVERY_TIMERS 8
struct EveryTimer {
    int           lua_ref;
    unsigned long interval_ms;
    unsigned long last_ms;
};
static EveryTimer g_timers[MAX_EVERY_TIMERS] = {};
static int        g_timer_count = 0;

// ui.on_tap() deferred event queue
#define MAX_PENDING_TAPS 4
#define MAX_TAP_WIDGETS  16
static int g_pending_taps[MAX_PENDING_TAPS] = {};
static int g_pending_tap_count = 0;
static int g_tap_refs[MAX_TAP_WIDGETS] = {};
static int g_tap_ref_count = 0;

bool sdkExitRequested() { return g_exit_requested; }
void sdkClearExitFlag()  { g_exit_requested = false; }

int sdkGetTickRef(lua_State* L) {
    (void)L;
    return g_tick_ref;
}

void sdkCleanup() {
    // Free all canvas PSRAM buffers
    for (int i = 0; i < g_canvas_buf_count; i++) {
        if (g_canvas_bufs[i]) { heap_caps_free(g_canvas_bufs[i]); g_canvas_bufs[i] = nullptr; }
    }
    g_canvas_buf_count = 0;

    // Schedule deletion of the app's LVGL screen.
    // lv_obj_del_async defers to the next lv_timer_handler() tick, by which
    // point setScreenState() has already loaded a different active screen.
    // Calling lv_obj_del() synchronously on the currently-active screen crashes.
    if (g_lua_screen) {
        lv_obj_delete_async(g_lua_screen);
        g_lua_screen = nullptr;
    }

    // Unref all interval timer callbacks
    if (g_L) {
        for (int i = 0; i < g_timer_count; i++) {
            if (g_timers[i].lua_ref != LUA_NOREF)
                luaL_unref(g_L, LUA_REGISTRYINDEX, g_timers[i].lua_ref);
        }
        // Unref all tap callbacks
        for (int i = 0; i < g_tap_ref_count; i++) {
            if (g_tap_refs[i] != LUA_NOREF)
                luaL_unref(g_L, LUA_REGISTRYINDEX, g_tap_refs[i]);
        }
    }
    g_timer_count     = 0;
    g_tap_ref_count   = 0;
    g_pending_tap_count = 0;

    g_exit_requested = false;
    g_tick_ref       = LUA_NOREF;
}

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
static inline lv_color_t c565(uint16_t rgb565) {
    uint8_t r = ((rgb565 >> 11) & 0x1F) * 8;
    uint8_t g = ((rgb565 >> 5)  & 0x3F) * 4;
    uint8_t b = (rgb565 & 0x1F) * 8;
    return lv_color_make(r, g, b);
}

static const lv_font_t* font_by_size(int sz) {
    if (sz <= 14) return &lv_font_montserrat_14;
    if (sz <= 16) return &lv_font_montserrat_16;
    if (sz <= 20) return &lv_font_montserrat_20;
    if (sz <= 28) return &lv_font_montserrat_28;
    return &lv_font_montserrat_40;
}

static lv_align_t parse_align(const char* s) {
    if (!s) return LV_ALIGN_DEFAULT;
    if (strcmp(s, "center")      == 0) return LV_ALIGN_CENTER;
    if (strcmp(s, "top_mid")     == 0) return LV_ALIGN_TOP_MID;
    if (strcmp(s, "top_left")    == 0) return LV_ALIGN_TOP_LEFT;
    if (strcmp(s, "top_right")   == 0) return LV_ALIGN_TOP_RIGHT;
    if (strcmp(s, "bottom_mid")  == 0) return LV_ALIGN_BOTTOM_MID;
    if (strcmp(s, "bottom_left") == 0) return LV_ALIGN_BOTTOM_LEFT;
    if (strcmp(s, "bottom_right")== 0) return LV_ALIGN_BOTTOM_RIGHT;
    if (strcmp(s, "left_mid")    == 0) return LV_ALIGN_LEFT_MID;
    if (strcmp(s, "right_mid")   == 0) return LV_ALIGN_RIGHT_MID;
    return LV_ALIGN_DEFAULT;
}

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
static bool tbl_bool(lua_State* L, int idx, const char* key, bool def) {
    lua_getfield(L, idx, key);
    bool v = lua_isnil(L, -1) ? def : (bool)lua_toboolean(L, -1);
    lua_pop(L, 1);
    return v;
}

// ===========================================================================
//  bambu.* — printer state access (all fields)
// ===========================================================================

static int bambu_state(lua_State* L) {
    lua_pushstring(L, displayedPrinter().state.gcodeState); return 1; }
static int bambu_progress(lua_State* L) {
    lua_pushinteger(L, displayedPrinter().state.progress); return 1; }
static int bambu_nozzle_temp(lua_State* L) {
    lua_pushnumber(L, (lua_Number)displayedPrinter().state.nozzleTemp); return 1; }
static int bambu_bed_temp(lua_State* L) {
    lua_pushnumber(L, (lua_Number)displayedPrinter().state.bedTemp); return 1; }
static int bambu_chamber_temp(lua_State* L) {
    lua_pushnumber(L, (lua_Number)displayedPrinter().state.chamberTemp); return 1; }
static int bambu_nozzle_target(lua_State* L) {
    lua_pushnumber(L, (lua_Number)displayedPrinter().state.nozzleTarget); return 1; }
static int bambu_bed_target(lua_State* L) {
    lua_pushnumber(L, (lua_Number)displayedPrinter().state.bedTarget); return 1; }
static int bambu_remaining_mins(lua_State* L) {
    lua_pushinteger(L, displayedPrinter().state.remainingMinutes); return 1; }
static int bambu_layer(lua_State* L) {
    lua_pushinteger(L, displayedPrinter().state.layerNum); return 1; }
static int bambu_total_layers(lua_State* L) {
    lua_pushinteger(L, displayedPrinter().state.totalLayers); return 1; }
static int bambu_job_name(lua_State* L) {
    lua_pushstring(L, displayedPrinter().state.subtaskName); return 1; }
static int bambu_speed(lua_State* L) {
    lua_pushinteger(L, displayedPrinter().state.speedLevel); return 1; }
static int bambu_print_stage(lua_State* L) {
    lua_pushinteger(L, displayedPrinter().state.printStage); return 1; }
static int bambu_fan_part(lua_State* L) {
    lua_pushinteger(L, displayedPrinter().state.coolingFanPct); return 1; }
static int bambu_fan_aux(lua_State* L) {
    lua_pushinteger(L, displayedPrinter().state.auxFanPct); return 1; }
static int bambu_fan_chamber(lua_State* L) {
    lua_pushinteger(L, displayedPrinter().state.chamberFanPct); return 1; }
static int bambu_printer_name(lua_State* L) {
    lua_pushstring(L, displayedPrinter().config.name); return 1; }
static int bambu_connected(lua_State* L) {
    lua_pushboolean(L, displayedPrinter().state.connected ? 1 : 0); return 1; }
static int bambu_printing(lua_State* L) {
    lua_pushboolean(L, displayedPrinter().state.printing ? 1 : 0); return 1; }

// bambu.ams_color(tray_index) → RGB565 int, or 0 if not present
static int bambu_ams_color(lua_State* L) {
    int idx = (int)luaL_checkinteger(L, 1);
    const AmsState& ams = displayedPrinter().state.ams;
    if (idx >= 0 && idx < AMS_MAX_TRAYS && ams.trays[idx].present)
        lua_pushinteger(L, ams.trays[idx].colorRgb565);
    else
        lua_pushinteger(L, 0);
    return 1;
}

// bambu.ams_type(tray_index) → string
static int bambu_ams_type(lua_State* L) {
    int idx = (int)luaL_checkinteger(L, 1);
    const AmsState& ams = displayedPrinter().state.ams;
    if (idx >= 0 && idx < AMS_MAX_TRAYS && ams.trays[idx].present)
        lua_pushstring(L, ams.trays[idx].type);
    else
        lua_pushstring(L, "");
    return 1;
}

// bambu.ams_active() → tray index (0-15) or -1
static int bambu_ams_active(lua_State* L) {
    uint8_t t = displayedPrinter().state.ams.activeTray;
    lua_pushinteger(L, (t == 255) ? -1 : (long long)t);
    return 1;
}

static const luaL_Reg bambu_lib[] = {
    { "state",          bambu_state          },
    { "progress",       bambu_progress       },
    { "nozzle_temp",    bambu_nozzle_temp    },
    { "bed_temp",       bambu_bed_temp       },
    { "chamber_temp",   bambu_chamber_temp   },
    { "nozzle_target",  bambu_nozzle_target  },
    { "bed_target",     bambu_bed_target     },
    { "remaining_mins", bambu_remaining_mins },
    { "layer",          bambu_layer          },
    { "total_layers",   bambu_total_layers   },
    { "job_name",       bambu_job_name       },
    { "speed",          bambu_speed          },
    { "print_stage",    bambu_print_stage    },
    { "fan_part",       bambu_fan_part       },
    { "fan_aux",        bambu_fan_aux        },
    { "fan_chamber",    bambu_fan_chamber    },
    { "printer_name",   bambu_printer_name   },
    { "connected",      bambu_connected      },
    { "printing",       bambu_printing       },
    { "ams_color",      bambu_ams_color      },
    { "ams_type",       bambu_ams_type       },
    { "ams_active",     bambu_ams_active     },
    { nullptr,          nullptr              }
};

// ===========================================================================
//  sys.* — system utilities
// ===========================================================================

static int sys_millis(lua_State* L) {
    lua_pushinteger(L, (long long)millis()); return 1; }

static int sys_beep(lua_State* L) {
    (void)luaL_optnumber(L, 1, 1000.0);
    (void)luaL_optnumber(L, 2, 100.0);
    buzzerPlay(BUZZ_CONNECTED);
    return 0;
}

static int sys_log(lua_State* L) {
    Serial.println(luaL_checkstring(L, 1)); return 0; }

static int sys_exit(lua_State* L) {
    (void)L; g_exit_requested = true; return 0; }

static int sys_sdk_version(lua_State* L) {
    lua_pushinteger(L, SDK_VERSION); return 1; }

// sys.on_tick(fn) — register tick callback; fn(dt_ms) called each frame
static int sys_on_tick(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    if (g_tick_ref != LUA_NOREF)
        luaL_unref(L, LUA_REGISTRYINDEX, g_tick_ref);
    lua_pushvalue(L, 1);
    g_tick_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

// sys.every(ms, fn) — schedule fn to be called at most once per ms milliseconds
static int sys_every(lua_State* L) {
    unsigned long ms = (unsigned long)luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (g_timer_count >= MAX_EVERY_TIMERS) {
        luaL_error(L, "sys.every: max %d timers reached", MAX_EVERY_TIMERS);
        return 0;
    }
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    g_timers[g_timer_count++] = { ref, ms, (unsigned long)millis() };
    return 0;
}

// sys.http_get(url [, timeout_ms]) → body string or nil, err_string
static int sys_http_get(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    int timeout = (int)luaL_optinteger(L, 2, 8000);
    HTTPClient http;
    http.begin(url);
    http.setTimeout(timeout);
    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        http.end();
        lua_pushlstring(L, body.c_str(), body.length());
        return 1;
    }
    http.end();
    lua_pushnil(L);
    char errbuf[24];
    snprintf(errbuf, sizeof(errbuf), "HTTP %d", code);
    lua_pushstring(L, errbuf);
    return 2;
}

// sys.store_set(key, value) → bool  — persist string under /appdata/{key}
static int sys_store_set(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    const char* val = luaL_checkstring(L, 2);
    char path[64];
    snprintf(path, sizeof(path), "/appdata/%s", key);
    if (!LittleFS.exists("/appdata")) LittleFS.mkdir("/appdata");
    // Check free space (require at least 4 KB headroom)
    if (LittleFS.totalBytes() > 0 &&
        (LittleFS.totalBytes() - LittleFS.usedBytes()) < 4096) {
        lua_pushboolean(L, 0);
        return 1;
    }
    File f = LittleFS.open(path, "w");
    if (!f) { lua_pushboolean(L, 0); return 1; }
    f.print(val);
    f.close();
    lua_pushboolean(L, 1);
    return 1;
}

// sys.store_del(key) → bool  — delete a stored key
static int sys_store_del(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    char path[64];
    snprintf(path, sizeof(path), "/appdata/%s", key);
    bool ok = LittleFS.exists(path) && LittleFS.remove(path);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// sys.store_get(key [, default]) → string
static int sys_store_get(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    const char* def = luaL_optstring(L, 2, "");
    char path[48];
    snprintf(path, sizeof(path), "/appdata/%s", key);
    if (!LittleFS.exists(path)) {
        lua_pushstring(L, def);
        return 1;
    }
    File f = LittleFS.open(path, "r");
    if (!f) { lua_pushstring(L, def); return 1; }
    String v = f.readString();
    f.close();
    lua_pushlstring(L, v.c_str(), v.length());
    return 1;
}

// sys.time() → table {epoch, hour, min, sec, day, month, year, wday, synced}
static int sys_time(lua_State* L) {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    bool synced = (t.tm_year > (2020 - 1900));

    lua_newtable(L);
    lua_pushinteger(L, (long long)now);  lua_setfield(L, -2, "epoch");
    lua_pushinteger(L, t.tm_hour);       lua_setfield(L, -2, "hour");
    lua_pushinteger(L, t.tm_min);        lua_setfield(L, -2, "min");
    lua_pushinteger(L, t.tm_sec);        lua_setfield(L, -2, "sec");
    lua_pushinteger(L, t.tm_mday);       lua_setfield(L, -2, "day");
    lua_pushinteger(L, t.tm_mon + 1);    lua_setfield(L, -2, "month");
    lua_pushinteger(L, t.tm_year + 1900);lua_setfield(L, -2, "year");
    lua_pushinteger(L, t.tm_wday);       lua_setfield(L, -2, "wday");  // 0=Sun
    lua_pushboolean(L, synced ? 1 : 0); lua_setfield(L, -2, "synced");
    return 1;
}

// recursive helper: push JsonVariant as Lua value
static void json_to_lua(lua_State* L, JsonVariantConst v) {
    if (v.is<JsonObjectConst>()) {
        lua_newtable(L);
        for (auto kv : v.as<JsonObjectConst>()) {
            lua_pushstring(L, kv.key().c_str());
            json_to_lua(L, kv.value());
            lua_settable(L, -3);
        }
    } else if (v.is<JsonArrayConst>()) {
        lua_newtable(L);
        int i = 1;
        for (JsonVariantConst el : v.as<JsonArrayConst>()) {
            lua_pushinteger(L, i++);
            json_to_lua(L, el);
            lua_settable(L, -3);
        }
    } else if (v.is<bool>()) {
        lua_pushboolean(L, v.as<bool>() ? 1 : 0);
    } else if (v.is<long long>()) {
        lua_pushinteger(L, v.as<long long>());
    } else if (v.is<double>()) {
        lua_pushnumber(L, (lua_Number)v.as<double>());
    } else if (v.is<const char*>()) {
        lua_pushstring(L, v.as<const char*>());
    } else {
        lua_pushnil(L);
    }
}

// sys.json_parse(str) → table or nil, errmsg
static int sys_json_parse(lua_State* L) {
    const char* str = luaL_checkstring(L, 1);
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, str);
    if (err) {
        lua_pushnil(L);
        lua_pushstring(L, err.c_str());
        return 2;
    }
    json_to_lua(L, doc.as<JsonVariantConst>());
    return 1;
}

static const luaL_Reg sys_lib[] = {
    { "millis",      sys_millis      },
    { "beep",        sys_beep        },
    { "log",         sys_log         },
    { "exit",        sys_exit        },
    { "sdk_version", sys_sdk_version },
    { "on_tick",     sys_on_tick     },
    { "every",       sys_every       },
    { "http_get",    sys_http_get    },
    { "store_set",   sys_store_set   },
    { "store_get",   sys_store_get   },
    { "store_del",   sys_store_del   },
    { "time",        sys_time        },
    { "json_parse",  sys_json_parse  },
    { nullptr,       nullptr         }
};

// ===========================================================================
//  ui.* — LVGL widgets, all returning handles for live updates
// ===========================================================================

static int ui_screen(lua_State* L) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, c565(CLR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    // Track the first screen created — used for cleanup on exit
    if (!g_lua_screen) g_lua_screen = scr;
    // Show the screen immediately — single lv_scr_load, no separate ui.show() needed
    lv_scr_load(scr);
    lua_pushlightuserdata(L, scr);
    return 1;
}

// ui.label(parent, text, opts) → handle
static int ui_label(lua_State* L) {
    lv_obj_t* parent = (lv_obj_t*)lua_touserdata(L, 1);
    const char* text = luaL_checkstring(L, 2);
    if (!parent) return 0;

    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);

    if (lua_istable(L, 3)) {
        uint16_t    color   = (uint16_t)tbl_int(L, 3, "color", CLR_TEXT);
        int         font    = (int)     tbl_int(L, 3, "font",  16);
        const char* align_s = tbl_str(L, 3, "align", nullptr);
        int         ox      = (int)     tbl_int(L, 3, "x", 0);
        int         oy      = (int)     tbl_int(L, 3, "y", 0);
        int         w       = (int)     tbl_int(L, 3, "w", 0);

        lv_obj_set_style_text_color(lbl, c565(color), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, font_by_size(font), LV_PART_MAIN);

        if (w > 0) {
            lv_obj_set_width(lbl, w);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        }
        if (align_s) lv_obj_align(lbl, parse_align(align_s), ox, oy);
        else         lv_obj_set_pos(lbl, ox, oy);
    }
    lua_pushlightuserdata(L, lbl);
    return 1;
}

// ui.label_set(handle, text) — update label text
static int ui_label_set(lua_State* L) {
    lv_obj_t* lbl = (lv_obj_t*)lua_touserdata(L, 1);
    const char* text = luaL_checkstring(L, 2);
    if (lbl) lv_label_set_text(lbl, text);
    return 0;
}

// ui.label_color(handle, rgb565)
static int ui_label_color(lua_State* L) {
    lv_obj_t* lbl  = (lv_obj_t*)lua_touserdata(L, 1);
    uint16_t color = (uint16_t)luaL_checkinteger(L, 2);
    if (lbl) lv_obj_set_style_text_color(lbl, c565(color), LV_PART_MAIN);
    return 0;
}

// ui.arc(parent, opts) → handle
static int ui_arc(lua_State* L) {
    lv_obj_t* parent = (lv_obj_t*)lua_touserdata(L, 1);
    if (!parent || !lua_istable(L, 2)) return 0;

    int      value  = (int)     tbl_int(L, 2, "value",  0);
    int      size   = (int)     tbl_int(L, 2, "size",   80);
    uint16_t color  = (uint16_t)tbl_int(L, 2, "color",  CLR_GREEN);
    uint16_t track  = (uint16_t)tbl_int(L, 2, "track",  CLR_TRACK);
    int      cx     = (int)     tbl_int(L, 2, "cx",     120);
    int      cy     = (int)     tbl_int(L, 2, "cy",     120);
    int      width  = (int)     tbl_int(L, 2, "thickness", 8);
    int      start_a= (int)     tbl_int(L, 2, "start",  135);
    int      sweep  = (int)     tbl_int(L, 2, "sweep",  270);

    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size * 2, size * 2);
    int end_a = start_a + sweep;
    lv_arc_set_bg_angles(arc, start_a, end_a % 360);
    lv_arc_set_angles(arc, start_a, start_a + (int)((long long)sweep * value / 100));
    lv_arc_set_value(arc, value);

    lv_obj_set_style_arc_color(arc, c565(color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, c565(track),  LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(arc, cx - size, cy - size);
    lua_pushlightuserdata(L, arc);
    return 1;
}

// ui.arc_set(handle, value 0-100)
static int ui_arc_set(lua_State* L) {
    lv_obj_t* arc  = (lv_obj_t*)lua_touserdata(L, 1);
    int       val  = (int)luaL_checkinteger(L, 2);
    if (!arc) return 0;
    // Read back sweep from existing bg angles
    lv_arc_set_value(arc, val);
    return 0;
}

// ui.arc_color(handle, rgb565)
static int ui_arc_color(lua_State* L) {
    lv_obj_t* arc  = (lv_obj_t*)lua_touserdata(L, 1);
    uint16_t color = (uint16_t)luaL_checkinteger(L, 2);
    if (arc) lv_obj_set_style_arc_color(arc, c565(color), LV_PART_INDICATOR);
    return 0;
}

// ui.rect(parent, opts) → handle
static int ui_rect(lua_State* L) {
    lv_obj_t* parent = (lv_obj_t*)lua_touserdata(L, 1);
    if (!parent || !lua_istable(L, 2)) return 0;

    int      cx    = (int)     tbl_int(L, 2, "cx",    120);
    int      cy    = (int)     tbl_int(L, 2, "cy",    120);
    int      w     = (int)     tbl_int(L, 2, "w",      40);
    int      h     = (int)     tbl_int(L, 2, "h",      40);
    uint16_t color = (uint16_t)tbl_int(L, 2, "color", CLR_BTN);
    int      r     = (int)     tbl_int(L, 2, "radius", 0);
    bool     border= tbl_bool(L, 2, "border", false);
    uint16_t bcol  = (uint16_t)tbl_int(L, 2, "border_color", CLR_TEXT_DARK);

    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);  // strip default theme (border, outline, shadow, scroll)
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, cx - w / 2, cy - h / 2);
    lv_obj_set_style_bg_color(obj, c565(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, border ? 1 : 0, LV_PART_MAIN);
    if (border) lv_obj_set_style_border_color(obj, c565(bcol), LV_PART_MAIN);
    lv_obj_set_style_radius(obj, r, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lua_pushlightuserdata(L, obj);
    return 1;
}

// ui.rect_set(handle, color)
static int ui_rect_set(lua_State* L) {
    lv_obj_t* obj  = (lv_obj_t*)lua_touserdata(L, 1);
    uint16_t color = (uint16_t)luaL_checkinteger(L, 2);
    if (obj) lv_obj_set_style_bg_color(obj, c565(color), LV_PART_MAIN);
    return 0;
}

// ui.rect_size(handle, w, h)
static int ui_rect_size(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    int w = (int)luaL_checkinteger(L, 2);
    int h = (int)luaL_checkinteger(L, 3);
    if (obj) lv_obj_set_size(obj, w, h);
    return 0;
}

// Opa wrapper — lv_obj_set_style_opa needs a selector arg, unusable directly as anim cb
static void _set_opa(lv_obj_t* obj, int32_t v) {
    lv_obj_set_style_opa(obj, (lv_opa_t)v, LV_PART_MAIN);
}

// Arc value wrapper — lv_arc_set_value takes int16_t
static void _set_arc_val(lv_obj_t* obj, int32_t v) {
    lv_arc_set_value(obj, (int16_t)v);
}

static lv_anim_path_cb_t _resolve_easing(const char* s) {
    if (!s || strcmp(s, "ease_in_out") == 0) return lv_anim_path_ease_in_out;
    if (strcmp(s, "linear")    == 0) return lv_anim_path_linear;
    if (strcmp(s, "ease_in")   == 0) return lv_anim_path_ease_in;
    if (strcmp(s, "ease_out")  == 0) return lv_anim_path_ease_out;
    if (strcmp(s, "overshoot") == 0) return lv_anim_path_overshoot;
    if (strcmp(s, "bounce")    == 0) return lv_anim_path_bounce;
    if (strcmp(s, "step")      == 0) return lv_anim_path_step;
    return lv_anim_path_ease_in_out;
}

static lv_anim_path_cb_t tbl_easing(lua_State* L, int idx) {
    lua_getfield(L, idx, "easing");
    const char* s = lua_isstring(L, -1) ? lua_tostring(L, -1) : NULL;
    lv_anim_path_cb_t cb = _resolve_easing(s);
    lua_pop(L, 1);
    return cb;
}

static void _start_anim(lv_obj_t* obj, lv_anim_exec_xcb_t exec_cb,
                        int from, int to, int time_ms, int delay_ms,
                        bool rep, bool bounce, lv_anim_path_cb_t path_cb) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, time_ms);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_path_cb(&a, path_cb);
    if (bounce) lv_anim_set_playback_time(&a, time_ms);
    if (rep)    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

// ui.anim_fade(handle, from, to [, opts])
// opts: time, delay, repeat, bounce, easing
static int ui_anim_fade(lua_State* L) {
    lv_obj_t* obj  = (lv_obj_t*)lua_touserdata(L, 1);
    int       from = (int)luaL_checkinteger(L, 2);
    int       to   = (int)luaL_checkinteger(L, 3);
    if (!obj) return 0;
    bool tbl = lua_istable(L, 4);
    int  time_ms  = tbl ? (int)tbl_int(L, 4, "time",   300) : 300;
    int  delay_ms = tbl ? (int)tbl_int(L, 4, "delay",    0) : 0;
    bool rep      = tbl ? tbl_bool(L, 4, "repeat", false)   : false;
    bool bounce   = tbl ? tbl_bool(L, 4, "bounce", false)   : false;
    lv_anim_path_cb_t path = tbl ? tbl_easing(L, 4) : lv_anim_path_ease_in_out;
    _start_anim(obj, (lv_anim_exec_xcb_t)_set_opa, from, to, time_ms, delay_ms, rep, bounce, path);
    return 0;
}

// ui.anim_move(handle, x, y [, opts])
// opts: time, delay, repeat, bounce, easing
static int ui_anim_move(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    int       x   = (int)luaL_checkinteger(L, 2);
    int       y   = (int)luaL_checkinteger(L, 3);
    if (!obj) return 0;
    bool tbl = lua_istable(L, 4);
    int  time_ms  = tbl ? (int)tbl_int(L, 4, "time",   300) : 300;
    int  delay_ms = tbl ? (int)tbl_int(L, 4, "delay",    0) : 0;
    bool rep      = tbl ? tbl_bool(L, 4, "repeat", false)   : false;
    bool bounce   = tbl ? tbl_bool(L, 4, "bounce", false)   : false;
    lv_anim_path_cb_t path = tbl ? tbl_easing(L, 4) : lv_anim_path_ease_in_out;
    int cur_x = lv_obj_get_x(obj);
    int cur_y = lv_obj_get_y(obj);
    _start_anim(obj, (lv_anim_exec_xcb_t)lv_obj_set_x, cur_x, x, time_ms, delay_ms, rep, bounce, path);
    _start_anim(obj, (lv_anim_exec_xcb_t)lv_obj_set_y, cur_y, y, time_ms, delay_ms, rep, bounce, path);
    return 0;
}

// ui.anim_stop(handle) — cancel all running animations on a widget
static int ui_anim_stop(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    if (obj) lv_anim_del(obj, NULL);
    return 0;
}

// ui.anim_arc(handle, from, to [, opts]) — animate arc value 0–100
// opts: time (default 500), delay, repeat, bounce, easing
static int ui_anim_arc(lua_State* L) {
    lv_obj_t* obj  = (lv_obj_t*)lua_touserdata(L, 1);
    int       from = (int)luaL_checkinteger(L, 2);
    int       to   = (int)luaL_checkinteger(L, 3);
    if (!obj) return 0;
    bool tbl = lua_istable(L, 4);
    int  time_ms  = tbl ? (int)tbl_int(L, 4, "time",   500) : 500;
    int  delay_ms = tbl ? (int)tbl_int(L, 4, "delay",    0) : 0;
    bool rep      = tbl ? tbl_bool(L, 4, "repeat", false)   : false;
    bool bounce   = tbl ? tbl_bool(L, 4, "bounce", false)   : false;
    lv_anim_path_cb_t path = tbl ? tbl_easing(L, 4) : lv_anim_path_ease_in_out;
    _start_anim(obj, (lv_anim_exec_xcb_t)_set_arc_val, from, to, time_ms, delay_ms, rep, bounce, path);
    return 0;
}

// ui.anim_size(handle, w, h [, opts]) — animate widget dimensions
// opts: time (default 300), delay, repeat, bounce, easing
static int ui_anim_size(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    int       w   = (int)luaL_checkinteger(L, 2);
    int       h   = (int)luaL_checkinteger(L, 3);
    if (!obj) return 0;
    bool tbl = lua_istable(L, 4);
    int  time_ms  = tbl ? (int)tbl_int(L, 4, "time",   300) : 300;
    int  delay_ms = tbl ? (int)tbl_int(L, 4, "delay",    0) : 0;
    bool rep      = tbl ? tbl_bool(L, 4, "repeat", false)   : false;
    bool bounce   = tbl ? tbl_bool(L, 4, "bounce", false)   : false;
    lv_anim_path_cb_t path = tbl ? tbl_easing(L, 4) : lv_anim_path_ease_in_out;
    int cur_w = lv_obj_get_width(obj);
    int cur_h = lv_obj_get_height(obj);
    _start_anim(obj, (lv_anim_exec_xcb_t)lv_obj_set_width,  cur_w, w, time_ms, delay_ms, rep, bounce, path);
    _start_anim(obj, (lv_anim_exec_xcb_t)lv_obj_set_height, cur_h, h, time_ms, delay_ms, rep, bounce, path);
    return 0;
}

// LVGL 9 layer-based canvas draw helpers
#define CANVAS_LAYER_BEGIN(canvas, layer) \
    lv_layer_t layer; lv_canvas_init_layer(canvas, &layer)
#define CANVAS_LAYER_END(canvas, layer) \
    lv_canvas_finish_layer(canvas, &layer)

// ui.canvas(parent, w, h [,x ,y]) → handle  (draws into PSRAM buffer)
static int ui_canvas(lua_State* L) {
    lv_obj_t* parent = (lv_obj_t*)lua_touserdata(L, 1);
    int w = (int)luaL_checkinteger(L, 2);
    int h = (int)luaL_checkinteger(L, 3);
    int x = (int)luaL_optinteger(L, 4, 0);
    int y = (int)luaL_optinteger(L, 5, 0);
    if (!parent || w <= 0 || h <= 0) return 0;

    uint32_t stride   = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);
    size_t   buf_size = stride * h;
    void* buf = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!buf) { lua_pushnil(L); return 1; }
    if (g_canvas_buf_count < MAX_CANVAS_BUFS)
        g_canvas_bufs[g_canvas_buf_count++] = buf;

    lv_obj_t* canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, buf, w, h, LV_COLOR_FORMAT_RGB565);
    lv_canvas_fill_bg(canvas, c565(CLR_BG), LV_OPA_COVER);
    lv_obj_set_style_pad_all(canvas, 0, LV_PART_MAIN);
    lv_obj_set_pos(canvas, x, y);
    lua_pushlightuserdata(L, canvas);
    return 1;
}

// ui.canvas_line(canvas, x1,y1, x2,y2, color, width)
static int ui_canvas_line(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    if (!canvas) return 0;
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = c565((uint16_t)luaL_checkinteger(L, 6));
    dsc.width = (int32_t)(luaL_checkinteger(L, 7) > 0 ? luaL_checkinteger(L, 7) : 1);
    dsc.opa   = LV_OPA_COVER;
    dsc.p1.x  = (lv_value_precise_t)luaL_checkinteger(L, 2);
    dsc.p1.y  = (lv_value_precise_t)luaL_checkinteger(L, 3);
    dsc.p2.x  = (lv_value_precise_t)luaL_checkinteger(L, 4);
    dsc.p2.y  = (lv_value_precise_t)luaL_checkinteger(L, 5);
    CANVAS_LAYER_BEGIN(canvas, layer);
    lv_draw_line(&layer, &dsc);
    CANVAS_LAYER_END(canvas, layer);
    return 0;
}

// ui.canvas_rect(canvas, x, y, w, h, color [, radius])
static int ui_canvas_rect(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    if (!canvas) return 0;
    int x = (int)luaL_checkinteger(L, 2), y = (int)luaL_checkinteger(L, 3);
    int w = (int)luaL_checkinteger(L, 4), h = (int)luaL_checkinteger(L, 5);
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color     = c565((uint16_t)luaL_optinteger(L, 6, CLR_BTN));
    dsc.bg_opa       = LV_OPA_COVER;
    dsc.radius       = (int32_t)luaL_optinteger(L, 7, 0);
    dsc.border_width = 0;
    lv_area_t area   = { x, y, x + w - 1, y + h - 1 };
    CANVAS_LAYER_BEGIN(canvas, layer);
    lv_draw_rect(&layer, &dsc, &area);
    CANVAS_LAYER_END(canvas, layer);
    return 0;
}

// Helper: draw an arc as polyline segments using the LVGL 9 layer API.
static void canvas_draw_arc_polyline(lv_obj_t* canvas,
                                     float cx, float cy, float r,
                                     float a1_deg, float a2_deg,
                                     lv_color_t color, int width)
{
    float span = a2_deg - a1_deg;
    if (span <= 0) span += 360.0f;
    int segs = (int)(span / 6.0f);
    if (segs < 2)  segs = 2;
    if (segs > 64) segs = 64;

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color;
    dsc.width = (int32_t)(width > 0 ? width : 1);
    dsc.opa   = LV_OPA_COVER;

    float step   = span / (float)segs;
    float prev_x = cx + r * cosf(a1_deg * (float)M_PI / 180.0f);
    float prev_y = cy + r * sinf(a1_deg * (float)M_PI / 180.0f);
    for (int i = 1; i <= segs; i++) {
        float a  = a1_deg + step * (float)i;
        float nx = cx + r * cosf(a * (float)M_PI / 180.0f);
        float ny = cy + r * sinf(a * (float)M_PI / 180.0f);
        dsc.p1.x = (lv_value_precise_t)prev_x;
        dsc.p1.y = (lv_value_precise_t)prev_y;
        dsc.p2.x = (lv_value_precise_t)nx;
        dsc.p2.y = (lv_value_precise_t)ny;
        CANVAS_LAYER_BEGIN(canvas, layer);
        lv_draw_line(&layer, &dsc);
        CANVAS_LAYER_END(canvas, layer);
        prev_x = nx;
        prev_y = ny;
    }
}

// ui.canvas_circle(canvas, cx, cy, r, color [, thickness])
static int ui_canvas_circle(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    if (!canvas) return 0;
    float    cx    = (float)luaL_checkinteger(L, 2);
    float    cy    = (float)luaL_checkinteger(L, 3);
    float    r     = (float)luaL_checkinteger(L, 4);
    uint16_t color = (uint16_t)luaL_optinteger(L, 5, CLR_TEXT);
    int      width = (int)luaL_optinteger(L, 6, 1);
    canvas_draw_arc_polyline(canvas, cx, cy, r, 0.0f, 360.0f, c565(color), width);
    return 0;
}

// ui.canvas_arc(canvas, cx, cy, r, a1, a2, color [, thickness])
// angles in degrees, 0 = right (3 o'clock), clockwise
static int ui_canvas_arc(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    if (!canvas) return 0;
    float    cx    = (float)luaL_checkinteger(L, 2);
    float    cy    = (float)luaL_checkinteger(L, 3);
    float    r     = (float)luaL_checkinteger(L, 4);
    float    a1    = (float)luaL_checkinteger(L, 5);
    float    a2    = (float)luaL_checkinteger(L, 6);
    uint16_t color = (uint16_t)luaL_optinteger(L, 7, CLR_TEXT);
    int      width = (int)luaL_optinteger(L, 8, 1);
    canvas_draw_arc_polyline(canvas, cx, cy, r, a1, a2, c565(color), width);
    return 0;
}

// ui.canvas_polyline(canvas, points, color, width)
// points: flat Lua table {x1,y1, x2,y2, ...}
static int ui_canvas_polyline(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    if (!canvas) return 0;
    luaL_checktype(L, 2, LUA_TTABLE);
    uint16_t color = (uint16_t)luaL_optinteger(L, 3, CLR_TEXT);
    int      width = (int)luaL_optinteger(L, 4, 1);
    int n = (int)lua_rawlen(L, 2), npts = n / 2;
    if (npts < 2) return 0;

    lv_point_precise_t stack_pts[64];
    lv_point_precise_t *pts = (npts <= 64) ? stack_pts
        : (lv_point_precise_t*)malloc(npts * sizeof(lv_point_precise_t));
    if (!pts) return 0;
    for (int i = 0; i < npts; i++) {
        lua_rawgeti(L, 2, i*2+1); pts[i].x = (lv_value_precise_t)lua_tointeger(L, -1); lua_pop(L, 1);
        lua_rawgeti(L, 2, i*2+2); pts[i].y = (lv_value_precise_t)lua_tointeger(L, -1); lua_pop(L, 1);
    }
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = c565(color);
    dsc.width = (int32_t)(width > 0 ? width : 1);
    dsc.opa   = LV_OPA_COVER;
    for (int i = 0; i < npts - 1; i++) {
        dsc.p1 = pts[i]; dsc.p2 = pts[i + 1];
        CANVAS_LAYER_BEGIN(canvas, layer);
        lv_draw_line(&layer, &dsc);
        CANVAS_LAYER_END(canvas, layer);
    }
    if (pts != stack_pts) free(pts);
    return 0;
}

// ui.canvas_bezier(canvas, x0,y0, cx1,cy1, cx2,cy2, x1,y1 [,color, width, steps])
// Cubic Bezier via lv_bezier3 sampling.
static int ui_canvas_bezier(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    if (!canvas) return 0;
    int x0  = (int)luaL_checkinteger(L, 2),  y0  = (int)luaL_checkinteger(L, 3);
    int cx1 = (int)luaL_checkinteger(L, 4),  cy1 = (int)luaL_checkinteger(L, 5);
    int cx2 = (int)luaL_checkinteger(L, 6),  cy2 = (int)luaL_checkinteger(L, 7);
    int x1  = (int)luaL_checkinteger(L, 8),  y1  = (int)luaL_checkinteger(L, 9);
    uint16_t color = (uint16_t)luaL_optinteger(L, 10, CLR_TEXT);
    int      width = (int)luaL_optinteger(L, 11, 1);
    int      steps = (int)luaL_optinteger(L, 12, 20);
    if (steps < 2)  steps = 2;
    if (steps > 64) steps = 64;

    lv_point_precise_t pts[65];
    for (int i = 0; i <= steps; i++) {
        uint32_t t = (uint32_t)(i * 1024 / steps);
        pts[i].x = (lv_value_precise_t)lv_bezier3(t, x0, cx1, cx2, x1);
        pts[i].y = (lv_value_precise_t)lv_bezier3(t, y0, cy1, cy2, y1);
    }
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = c565(color);
    dsc.width = (int32_t)(width > 0 ? width : 1);
    dsc.opa   = LV_OPA_COVER;
    for (int i = 0; i < steps; i++) {
        dsc.p1 = pts[i]; dsc.p2 = pts[i + 1];
        CANVAS_LAYER_BEGIN(canvas, layer);
        lv_draw_line(&layer, &dsc);
        CANVAS_LAYER_END(canvas, layer);
    }
    return 0;
}

// ui.canvas_clear(canvas, color)
static int ui_canvas_clear(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    uint16_t color = (uint16_t)luaL_optinteger(L, 2, CLR_BG);
    if (canvas) lv_canvas_fill_bg(canvas, c565(color), LV_OPA_COVER);
    return 0;
}


// ui.color(r, g, b) → rgb565 integer  — convert 0-255 RGB to RGB565
static int ui_color(lua_State* L) {
    int r = (int)luaL_checkinteger(L, 1);
    int g = (int)luaL_checkinteger(L, 2);
    int b = (int)luaL_checkinteger(L, 3);
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    lua_pushinteger(L, ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    return 1;
}

// LVGL event callback — enqueues the Lua ref, never calls Lua directly.
// Called from lv_timer_handler() inside lvglPortTick(), before luaRuntimeTick().
static void tap_event_cb(lv_event_t* e) {
    int ref = (int)(intptr_t)lv_event_get_user_data(e);
    if (g_pending_tap_count < MAX_PENDING_TAPS)
        g_pending_taps[g_pending_tap_count++] = ref;
}

// ui.on_tap(widget, fn) — register a tap callback for any widget
static int ui_on_tap(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (!obj) return 0;
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj, tap_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)ref);
    if (g_tap_ref_count < MAX_TAP_WIDGETS)
        g_tap_refs[g_tap_ref_count++] = ref;
    return 0;
}

// ui.delete(handle) — clean up a widget
static int ui_obj_delete(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    if (obj) lv_obj_delete(obj);
    return 0;
}

static const luaL_Reg ui_lib[] = {
    { "screen",        ui_screen        },
    { "label",         ui_label         },
    { "label_set",     ui_label_set     },
    { "label_color",   ui_label_color   },
    { "arc",           ui_arc           },
    { "arc_set",       ui_arc_set       },
    { "arc_color",     ui_arc_color     },
    { "rect",          ui_rect          },
    { "rect_set",      ui_rect_set      },
    { "rect_size",     ui_rect_size     },
    { "anim_fade",     ui_anim_fade     },
    { "anim_move",     ui_anim_move     },
    { "anim_stop",     ui_anim_stop     },
    { "anim_arc",      ui_anim_arc      },
    { "anim_size",     ui_anim_size     },
    { "canvas",        ui_canvas        },
    { "canvas_line",   ui_canvas_line   },
    { "canvas_rect",   ui_canvas_rect   },
    { "canvas_circle",   ui_canvas_circle   },
    { "canvas_arc",      ui_canvas_arc      },
    { "canvas_polyline", ui_canvas_polyline },
    { "canvas_bezier",   ui_canvas_bezier   },
    { "canvas_clear",    ui_canvas_clear    },
    { "color",         ui_color         },
    { "on_tap",        ui_on_tap        },
    { "delete",        ui_obj_delete    },
    { nullptr,         nullptr          }
};

// ===========================================================================
//  Exported runtime helpers — called from lua_runtime.cpp each tick
// ===========================================================================

void sdkTickTimers(lua_State* L) {
    unsigned long now = (unsigned long)millis();
    for (int i = 0; i < g_timer_count; i++) {
        EveryTimer& t = g_timers[i];
        if (now - t.last_ms >= t.interval_ms) {
            t.last_ms = now;
            lua_rawgeti(L, LUA_REGISTRYINDEX, t.lua_ref);
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                Serial.printf("LuaRuntime: sys.every error: %s\n", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }
    }
}

void sdkDrainTapQueue(lua_State* L) {
    int count = g_pending_tap_count;
    g_pending_tap_count = 0;
    for (int i = 0; i < count; i++) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_pending_taps[i]);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            Serial.printf("LuaRuntime: on_tap error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

// ===========================================================================
//  imu.* — QMI8658 6-axis IMU (onboard, shared I2C bus GPIO6/GPIO7)
// ===========================================================================

static SensorQMI8658 s_qmi;
static bool          s_imuReady = false;

// Called once from main.cpp setup() after loadSettings().
// Declared here; exposed via sdk_api.h via imuInit().
void imuInit() {
  if (s_qmi.begin(Wire, IMU_I2C_ADDR, TOUCH_SDA, TOUCH_SCL)) {
    s_qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_8G, SensorQMI8658::ACC_ODR_1000Hz, SensorQMI8658::LPF_MODE_0);
    s_qmi.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS, SensorQMI8658::GYR_ODR_896_8Hz, SensorQMI8658::LPF_MODE_0);
    s_qmi.enableAccelerometer();
    s_qmi.enableGyroscope();
    s_imuReady = true;
    Serial.println("[imu] QMI8658 init OK");
  } else {
    Serial.println("[imu] QMI8658 init FAILED");
  }
}

static int imu_ready(lua_State* L) {
  lua_pushboolean(L, s_imuReady && s_qmi.getDataReady());
  return 1;
}

static int imu_accel(lua_State* L) {
  if (!s_imuReady) { lua_pushnil(L); return 1; }
  float x = 0, y = 0, z = 0;
  s_qmi.getAccelerometer(x, y, z);
  lua_newtable(L);
  lua_pushnumber(L, x); lua_setfield(L, -2, "x");
  lua_pushnumber(L, y); lua_setfield(L, -2, "y");
  lua_pushnumber(L, z); lua_setfield(L, -2, "z");
  return 1;
}

static int imu_gyro(lua_State* L) {
  if (!s_imuReady) { lua_pushnil(L); return 1; }
  float x = 0, y = 0, z = 0;
  s_qmi.getGyroscope(x, y, z);
  lua_newtable(L);
  lua_pushnumber(L, x); lua_setfield(L, -2, "x");
  lua_pushnumber(L, y); lua_setfield(L, -2, "y");
  lua_pushnumber(L, z); lua_setfield(L, -2, "z");
  return 1;
}

static int imu_temp(lua_State* L) {
  if (!s_imuReady) { lua_pushnumber(L, 0); return 1; }
  float t = s_qmi.getTemperature_C();
  lua_pushnumber(L, t);
  return 1;
}

// Convenience: compute pitch and roll from accelerometer
static int imu_tilt(lua_State* L) {
  if (!s_imuReady) { lua_pushnil(L); return 1; }
  float x = 0, y = 0, z = 0;
  s_qmi.getAccelerometer(x, y, z);
  float pitch = atan2f(-x, sqrtf(y * y + z * z)) * 57.2957795f;
  float roll  = atan2f(y, z) * 57.2957795f;
  lua_newtable(L);
  lua_pushnumber(L, pitch); lua_setfield(L, -2, "pitch");
  lua_pushnumber(L, roll);  lua_setfield(L, -2, "roll");
  return 1;
}

// True if instantaneous acceleration magnitude is outside the 0.8–1.2 g band
static int imu_shake(lua_State* L) {
  if (!s_imuReady) { lua_pushboolean(L, false); return 1; }
  float x = 0, y = 0, z = 0;
  s_qmi.getAccelerometer(x, y, z);
  float mag = sqrtf(x * x + y * y + z * z);  // m/s²
  bool shaking = (mag < 7.85f || mag > 11.77f);  // outside ~0.8g–1.2g
  lua_pushboolean(L, shaking);
  return 1;
}

static const luaL_Reg imu_lib[] = {
  { "ready", imu_ready },
  { "accel", imu_accel },
  { "gyro",  imu_gyro  },
  { "temp",  imu_temp  },
  { "tilt",  imu_tilt  },
  { "shake", imu_shake },
  { nullptr, nullptr   }
};

// ===========================================================================
//  ble.* — BLE 5.0 central (scanner/client) for Lua apps
// ===========================================================================

static bool s_nimble_initialized = false;

// ble.scan(duration_ms) → table of {name, addr, rssi}
static int ble_scan(lua_State* L) {
  // Lazy init — NimBLEDevice::init() must be called before getScan() works.
  // initBLE() in ble_manager.cpp only runs when bleEnabled==true, so we guard here.
  if (!s_nimble_initialized) {
    NimBLEDevice::init(BLE_ADV_NAME);
    s_nimble_initialized = true;
  }

  int durationMs = (int)luaL_optinteger(L, 1, 1000);
  durationMs = constrain(durationMs, 100, 5000);

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(97);
  scan->setWindow(37);
  NimBLEScanResults results = scan->start((durationMs + 999) / 1000, false);

  lua_newtable(L);
  for (int i = 0; i < results.getCount(); i++) {
    NimBLEAdvertisedDevice dev = results.getDevice(i);
    lua_newtable(L);
    lua_pushstring(L, dev.getName().c_str());   lua_setfield(L, -2, "name");
    lua_pushstring(L, dev.getAddress().toString().c_str()); lua_setfield(L, -2, "addr");
    lua_pushinteger(L, dev.getRSSI());           lua_setfield(L, -2, "rssi");
    lua_rawseti(L, -2, i + 1);
  }
  scan->clearResults();
  return 1;
}

// ble.advertising() → bool
static int ble_advertising(lua_State* L) {
  lua_pushboolean(L, bleIsAdvertising());
  return 1;
}

// ble.set_advertising(bool)
static int ble_set_advertising(lua_State* L) {
  bool enable = lua_toboolean(L, 1);
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (enable) {
    adv->start();
  } else {
    adv->stop();
  }
  return 0;
}

static const luaL_Reg ble_lib[] = {
  { "scan",            ble_scan            },
  { "advertising",     ble_advertising     },
  { "set_advertising", ble_set_advertising },
  { nullptr,           nullptr             }
};

// ===========================================================================
//  Registration
// ===========================================================================

void sdkApiRegister(lua_State* L) {
    g_L              = L;   // cache for cleanup
    g_exit_requested = false;
    g_tick_ref       = LUA_NOREF;

    luaL_newlib(L, bambu_lib);  lua_setglobal(L, "bambu");
    luaL_newlib(L, sys_lib);    lua_setglobal(L, "sys");
    luaL_newlib(L, ui_lib);     lua_setglobal(L, "ui");
    luaL_newlib(L, imu_lib);    lua_setglobal(L, "imu");
    luaL_newlib(L, ble_lib);    lua_setglobal(L, "ble");
}

#endif // LUA_AVAILABLE
