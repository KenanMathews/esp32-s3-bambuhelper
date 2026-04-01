/**
 * sdk_ui.c — BambuHelper UI Module
 *
 * Implements the ui.* Lua API using LVGL 9.
 * Widgets are returned as light userdata (lv_obj_t*) to minimize overhead.
 *
 * Color convention: all color values accepted by this API are RGB565 integers
 * (e.g. 0xFFFF = white, 0x07E0 = green). They are converted to LVGL's
 * lv_color_t via rgb565_to_lvcolor().
 */

#include "sdk_ui.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <lvgl.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static lv_color_t rgb565_to_lvcolor(uint32_t rgb565)
{
    uint8_t r5 = (rgb565 >> 11) & 0x1F;
    uint8_t g6 = (rgb565 >>  5) & 0x3F;
    uint8_t b5 = (rgb565      ) & 0x1F;
    uint8_t r8 = (r5 << 3) | (r5 >> 2);
    uint8_t g8 = (g6 << 2) | (g6 >> 4);
    uint8_t b8 = (b5 << 3) | (b5 >> 2);
    return lv_color_make(r8, g8, b8);
}

static lv_align_t str_to_align(const char *s)
{
    if (!s) return LV_ALIGN_CENTER;
    if (strcmp(s, "top_mid")     == 0) return LV_ALIGN_TOP_MID;
    if (strcmp(s, "top_left")    == 0) return LV_ALIGN_TOP_LEFT;
    if (strcmp(s, "top_right")   == 0) return LV_ALIGN_TOP_RIGHT;
    if (strcmp(s, "center")      == 0) return LV_ALIGN_CENTER;
    if (strcmp(s, "bottom_mid")  == 0) return LV_ALIGN_BOTTOM_MID;
    if (strcmp(s, "bottom_left") == 0) return LV_ALIGN_BOTTOM_LEFT;
    if (strcmp(s, "bottom_right")== 0) return LV_ALIGN_BOTTOM_RIGHT;
    if (strcmp(s, "left_mid")    == 0) return LV_ALIGN_LEFT_MID;
    if (strcmp(s, "right_mid")   == 0) return LV_ALIGN_RIGHT_MID;
    return LV_ALIGN_CENTER;
}

static const lv_font_t *size_to_font(int size)
{
    switch (size) {
        case 14: return &lv_font_montserrat_14;
        case 16: return &lv_font_montserrat_16;
        case 20: return &lv_font_montserrat_20;
        case 28: return &lv_font_montserrat_28;
        case 40: return &lv_font_montserrat_40;
        default: return &lv_font_montserrat_14;
    }
}

/**
 * Map easing string to LVGL 9 path callback.
 */
static lv_anim_path_cb_t str_to_path(const char *s)
{
    if (!s) return lv_anim_path_ease_in_out;
    if (strcmp(s, "linear")       == 0) return lv_anim_path_linear;
    if (strcmp(s, "ease_in")      == 0) return lv_anim_path_ease_in;
    if (strcmp(s, "ease_out")     == 0) return lv_anim_path_ease_out;
    if (strcmp(s, "ease_in_out")  == 0) return lv_anim_path_ease_in_out;
    if (strcmp(s, "overshoot")    == 0) return lv_anim_path_overshoot;
    if (strcmp(s, "bounce")       == 0) return lv_anim_path_bounce;
    if (strcmp(s, "step")         == 0) return lv_anim_path_step;
    return lv_anim_path_ease_in_out;
}

/**
 * Parse common animation options from Lua table at stack index idx.
 * Fills: time_ms, delay_ms, repeat_flag, bounce_flag, path_cb.
 * Defaults: time=300, delay=0, repeat=false, bounce=false, easing=ease_in_out.
 */
static void parse_anim_opts(lua_State *L, int idx,
                             int *time_ms, int *delay_ms,
                             int *repeat_flag, int *bounce_flag,
                             lv_anim_path_cb_t *path_cb,
                             int default_time)
{
    *time_ms    = default_time;
    *delay_ms   = 0;
    *repeat_flag = 0;
    *bounce_flag = 0;
    *path_cb    = lv_anim_path_ease_in_out;

    if (!lua_istable(L, idx)) return;

    lua_getfield(L, idx, "time");
    if (lua_isnumber(L, -1)) *time_ms = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, idx, "delay");
    if (lua_isnumber(L, -1)) *delay_ms = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, idx, "repeat");
    if (lua_isboolean(L, -1)) *repeat_flag = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, idx, "bounce");
    if (lua_isboolean(L, -1)) *bounce_flag = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, idx, "easing");
    if (lua_isstring(L, -1)) *path_cb = str_to_path(lua_tostring(L, -1));
    lua_pop(L, 1);
}

/* -------------------------------------------------------------------------
 * Animation exec callbacks
 * ------------------------------------------------------------------------- */

static void anim_set_opa(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void anim_set_x(void *obj, int32_t v)
{
    lv_obj_set_x((lv_obj_t *)obj, v);
}

static void anim_set_y(void *obj, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)obj, v);
}

static void anim_set_arc_value(void *obj, int32_t v)
{
    lv_arc_set_value((lv_obj_t *)obj, v);
}

static void anim_set_width(void *obj, int32_t v)
{
    lv_obj_set_width((lv_obj_t *)obj, v);
}

static void anim_set_height(void *obj, int32_t v)
{
    lv_obj_set_height((lv_obj_t *)obj, v);
}

/* -------------------------------------------------------------------------
 * ui.screen()
 * ------------------------------------------------------------------------- */
static int l_screen(lua_State *L)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_screen_load(scr);
    lua_pushlightuserdata(L, scr);
    return 1;
}

/* -------------------------------------------------------------------------
 * ui.label(scr, text, opts)
 * ------------------------------------------------------------------------- */
static int l_label(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *scr  = (lv_obj_t *)lua_touserdata(L, 1);
    const char *text = luaL_checkstring(L, 2);

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, text);

    /* Transparent background */
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lbl, 0, 0);

    if (lua_istable(L, 3)) {
        lv_align_t align = LV_ALIGN_CENTER;
        int ox = 0, oy = 0;

        lua_getfield(L, 3, "align");
        if (lua_isstring(L, -1)) align = str_to_align(lua_tostring(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 3, "x");
        if (lua_isnumber(L, -1)) ox = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 3, "y");
        if (lua_isnumber(L, -1)) oy = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lv_obj_align(lbl, align, ox, oy);

        lua_getfield(L, 3, "color");
        if (lua_isnumber(L, -1)) {
            uint32_t rgb565 = (uint32_t)lua_tointeger(L, -1);
            lv_obj_set_style_text_color(lbl, rgb565_to_lvcolor(rgb565), 0);
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "font");
        if (lua_isnumber(L, -1)) {
            int size = (int)lua_tointeger(L, -1);
            lv_obj_set_style_text_font(lbl, size_to_font(size), 0);
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "w");
        if (lua_isnumber(L, -1)) {
            int w = (int)lua_tointeger(L, -1);
            lv_obj_set_width(lbl, w);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        }
        lua_pop(L, 1);
    }

    lua_pushlightuserdata(L, lbl);
    return 1;
}

/* -------------------------------------------------------------------------
 * ui.label_set(label, text)
 * ------------------------------------------------------------------------- */
static int l_label_set(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *lbl    = (lv_obj_t *)lua_touserdata(L, 1);
    const char *text = luaL_checkstring(L, 2);
    lv_label_set_text(lbl, text);
    return 0;
}

/* -------------------------------------------------------------------------
 * ui.label_color(label, color)
 * ------------------------------------------------------------------------- */
static int l_label_color(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *lbl   = (lv_obj_t *)lua_touserdata(L, 1);
    uint32_t rgb565 = (uint32_t)luaL_checkinteger(L, 2);
    lv_obj_set_style_text_color(lbl, rgb565_to_lvcolor(rgb565), 0);
    return 0;
}

/* -------------------------------------------------------------------------
 * ui.arc(scr, opts)
 * ------------------------------------------------------------------------- */
static int l_arc(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *scr = (lv_obj_t *)lua_touserdata(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    int cx = 120, cy = 120;
    int value = 0, min = 0, max = 100;
    int size = 80, thickness = 8;
    int start_angle = 135, sweep = 270;
    uint32_t color_rgb565 = 0x07E0;
    uint32_t track_rgb565 = 0x2104; /* dim grey */
    int has_color = 0, has_track = 0;

    lua_getfield(L, 2, "cx");
    if (lua_isnumber(L, -1)) cx = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "cy");
    if (lua_isnumber(L, -1)) cy = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "value");
    if (lua_isnumber(L, -1)) value = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "min");
    if (lua_isnumber(L, -1)) min = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "max");
    if (lua_isnumber(L, -1)) max = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "color");
    if (lua_isnumber(L, -1)) { color_rgb565 = (uint32_t)lua_tointeger(L, -1); has_color = 1; }
    lua_pop(L, 1);

    lua_getfield(L, 2, "track");
    if (lua_isnumber(L, -1)) { track_rgb565 = (uint32_t)lua_tointeger(L, -1); has_track = 1; }
    lua_pop(L, 1);

    lua_getfield(L, 2, "size");
    if (lua_isnumber(L, -1)) size = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "thickness");
    if (lua_isnumber(L, -1)) thickness = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "start");
    if (lua_isnumber(L, -1)) start_angle = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "sweep");
    if (lua_isnumber(L, -1)) sweep = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lv_obj_t *arc = lv_arc_create(scr);
    lv_obj_set_size(arc, size, size);

    int end_angle = start_angle + sweep;
    lv_arc_set_bg_angles(arc, start_angle, end_angle % 360);
    lv_arc_set_range(arc, min, max);
    lv_arc_set_value(arc, value);

    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    if (has_color) {
        lv_obj_set_style_arc_color(arc, rgb565_to_lvcolor(color_rgb565), LV_PART_INDICATOR);
    }
    if (has_track) {
        lv_obj_set_style_arc_color(arc, rgb565_to_lvcolor(track_rgb565), LV_PART_MAIN);
    }

    lv_obj_set_style_arc_width(arc, thickness, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, thickness, LV_PART_INDICATOR);

    lv_obj_set_pos(arc, cx - size / 2, cy - size / 2);

    lua_pushlightuserdata(L, arc);
    return 1;
}

/* -------------------------------------------------------------------------
 * ui.arc_set(arc, value)
 * ------------------------------------------------------------------------- */
static int l_arc_set(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *arc = (lv_obj_t *)lua_touserdata(L, 1);
    int value     = (int)luaL_checkinteger(L, 2);
    lv_arc_set_value(arc, value);
    return 0;
}

/* -------------------------------------------------------------------------
 * ui.arc_color(arc, color)
 * ------------------------------------------------------------------------- */
static int l_arc_color(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *arc   = (lv_obj_t *)lua_touserdata(L, 1);
    uint32_t rgb565 = (uint32_t)luaL_checkinteger(L, 2);
    lv_obj_set_style_arc_color(arc, rgb565_to_lvcolor(rgb565), LV_PART_INDICATOR);
    return 0;
}

/* -------------------------------------------------------------------------
 * ui.rect(scr, opts)
 *
 * Accepts cx/cy (centre, per spec) or x/y (top-left, legacy) for positioning.
 * ------------------------------------------------------------------------- */
static int l_rect(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *scr = (lv_obj_t *)lua_touserdata(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    int w = 40, h = 40, radius = 0;
    int has_cx = 0, has_x = 0;
    int cx = 120, cy = 120, rx = 0, ry = 0;
    uint32_t color_rgb565  = 0x39E7;
    uint32_t border_rgb565 = 0xC618;
    int border = 0;

    lua_getfield(L, 2, "cx");
    if (lua_isnumber(L, -1)) { cx = (int)lua_tointeger(L, -1); has_cx = 1; }
    lua_pop(L, 1);

    lua_getfield(L, 2, "cy");
    if (lua_isnumber(L, -1)) cy = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "x");
    if (lua_isnumber(L, -1)) { rx = (int)lua_tointeger(L, -1); has_x = 1; }
    lua_pop(L, 1);

    lua_getfield(L, 2, "y");
    if (lua_isnumber(L, -1)) ry = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "w");
    if (lua_isnumber(L, -1)) w = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "h");
    if (lua_isnumber(L, -1)) h = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "color");
    if (lua_isnumber(L, -1)) color_rgb565 = (uint32_t)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "radius");
    if (lua_isnumber(L, -1)) radius = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "border");
    if (lua_isboolean(L, -1)) border = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "border_color");
    if (lua_isnumber(L, -1)) border_rgb565 = (uint32_t)lua_tointeger(L, -1);
    lua_pop(L, 1);

    /* Determine final top-left position */
    int final_x, final_y;
    if (has_cx) {
        final_x = cx - w / 2;
        final_y = cy - h / 2;
    } else if (has_x) {
        final_x = rx;
        final_y = ry;
    } else {
        /* default: centred on screen */
        final_x = 120 - w / 2;
        final_y = 120 - h / 2;
    }

    lv_obj_t *rect = lv_obj_create(scr);
    lv_obj_set_size(rect, w, h);
    lv_obj_set_pos(rect, final_x, final_y);

    /* color=0x0000 means transparent (tap-zone idiom) */
    if (color_rgb565 == 0) {
        lv_obj_set_style_bg_opa(rect, LV_OPA_TRANSP, 0);
    } else {
        lv_obj_set_style_bg_color(rect, rgb565_to_lvcolor(color_rgb565), 0);
        lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);
    }
    lv_obj_set_style_radius(rect, radius, 0);
    lv_obj_set_style_pad_all(rect, 0, 0);
    lv_obj_clear_flag(rect, LV_OBJ_FLAG_SCROLLABLE);

    if (border) {
        lv_obj_set_style_border_width(rect, 1, 0);
        lv_obj_set_style_border_color(rect, rgb565_to_lvcolor(border_rgb565), 0);
    } else {
        lv_obj_set_style_border_width(rect, 0, 0);
    }

    lua_pushlightuserdata(L, rect);
    return 1;
}

/* -------------------------------------------------------------------------
 * ui.rect_set(rect, color)
 * ------------------------------------------------------------------------- */
static int l_rect_set(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *rect  = (lv_obj_t *)lua_touserdata(L, 1);
    uint32_t rgb565 = (uint32_t)luaL_checkinteger(L, 2);
    lv_obj_set_style_bg_color(rect, rgb565_to_lvcolor(rgb565), 0);
    return 0;
}

/* -------------------------------------------------------------------------
 * ui.rect_size(rect, w, h)
 * ------------------------------------------------------------------------- */
static int l_rect_size(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *rect = (lv_obj_t *)lua_touserdata(L, 1);
    int w = (int)luaL_checkinteger(L, 2);
    int h = (int)luaL_checkinteger(L, 3);
    lv_obj_set_size(rect, w, h);
    return 0;
}

/* -------------------------------------------------------------------------
 * ui.show(scr)
 * ------------------------------------------------------------------------- */
static int l_show(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *scr = (lv_obj_t *)lua_touserdata(L, 1);
    lv_screen_load(scr);
    return 0;
}

/* -------------------------------------------------------------------------
 * ui.delete(widget)
 * ------------------------------------------------------------------------- */
static int l_delete(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    lv_obj_delete(obj);
    return 0;
}

/* -------------------------------------------------------------------------
 * ui.color(r, g, b)
 * ------------------------------------------------------------------------- */
static int l_color(lua_State *L)
{
    int r = (int)luaL_checkinteger(L, 1);
    int g = (int)luaL_checkinteger(L, 2);
    int b = (int)luaL_checkinteger(L, 3);
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    uint32_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    lua_pushinteger(L, (lua_Integer)rgb565);
    return 1;
}

/* -------------------------------------------------------------------------
 * Animations
 * ------------------------------------------------------------------------- */

static int l_anim_fade(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    int from      = (int)luaL_checkinteger(L, 2);
    int to        = (int)luaL_checkinteger(L, 3);

    int time_ms, delay_ms, repeat_flag, bounce_flag;
    lv_anim_path_cb_t path_cb;
    parse_anim_opts(L, 4, &time_ms, &delay_ms, &repeat_flag, &bounce_flag, &path_cb, 300);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, anim_set_opa);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, time_ms);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_path_cb(&a, path_cb);
    if (repeat_flag) lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    if (bounce_flag) lv_anim_set_playback_duration(&a, time_ms);
    lv_anim_start(&a);
    return 0;
}

static int l_anim_move(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    int tx         = (int)luaL_checkinteger(L, 2);
    int ty         = (int)luaL_checkinteger(L, 3);

    int time_ms, delay_ms, repeat_flag, bounce_flag;
    lv_anim_path_cb_t path_cb;
    parse_anim_opts(L, 4, &time_ms, &delay_ms, &repeat_flag, &bounce_flag, &path_cb, 300);

    int32_t cur_x = lv_obj_get_x_aligned(obj);
    int32_t cur_y = lv_obj_get_y_aligned(obj);

    lv_anim_t ax, ay;
    lv_anim_init(&ax);
    lv_anim_set_var(&ax, obj);
    lv_anim_set_exec_cb(&ax, anim_set_x);
    lv_anim_set_values(&ax, cur_x, tx);
    lv_anim_set_duration(&ax, time_ms);
    lv_anim_set_delay(&ax, delay_ms);
    lv_anim_set_path_cb(&ax, path_cb);
    if (repeat_flag) lv_anim_set_repeat_count(&ax, LV_ANIM_REPEAT_INFINITE);
    if (bounce_flag) lv_anim_set_playback_duration(&ax, time_ms);
    lv_anim_start(&ax);

    lv_anim_init(&ay);
    lv_anim_set_var(&ay, obj);
    lv_anim_set_exec_cb(&ay, anim_set_y);
    lv_anim_set_values(&ay, cur_y, ty);
    lv_anim_set_duration(&ay, time_ms);
    lv_anim_set_delay(&ay, delay_ms);
    lv_anim_set_path_cb(&ay, path_cb);
    if (repeat_flag) lv_anim_set_repeat_count(&ay, LV_ANIM_REPEAT_INFINITE);
    if (bounce_flag) lv_anim_set_playback_duration(&ay, time_ms);
    lv_anim_start(&ay);

    return 0;
}

static int l_anim_stop(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    lv_anim_delete(obj, NULL);
    return 0;
}

static int l_anim_arc(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *arc = (lv_obj_t *)lua_touserdata(L, 1);
    int from      = (int)luaL_checkinteger(L, 2);
    int to        = (int)luaL_checkinteger(L, 3);

    int time_ms, delay_ms, repeat_flag, bounce_flag;
    lv_anim_path_cb_t path_cb;
    parse_anim_opts(L, 4, &time_ms, &delay_ms, &repeat_flag, &bounce_flag, &path_cb, 500);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, anim_set_arc_value);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, time_ms);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_path_cb(&a, path_cb);
    if (repeat_flag) lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    if (bounce_flag) lv_anim_set_playback_duration(&a, time_ms);
    lv_anim_start(&a);
    return 0;
}

static int l_anim_size(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    int tw         = (int)luaL_checkinteger(L, 2);
    int th         = (int)luaL_checkinteger(L, 3);

    int time_ms, delay_ms, repeat_flag, bounce_flag;
    lv_anim_path_cb_t path_cb;
    parse_anim_opts(L, 4, &time_ms, &delay_ms, &repeat_flag, &bounce_flag, &path_cb, 300);

    int32_t cur_w = lv_obj_get_width(obj);
    int32_t cur_h = lv_obj_get_height(obj);

    lv_anim_t aw, ah;
    lv_anim_init(&aw);
    lv_anim_set_var(&aw, obj);
    lv_anim_set_exec_cb(&aw, anim_set_width);
    lv_anim_set_values(&aw, cur_w, tw);
    lv_anim_set_duration(&aw, time_ms);
    lv_anim_set_delay(&aw, delay_ms);
    lv_anim_set_path_cb(&aw, path_cb);
    if (repeat_flag) lv_anim_set_repeat_count(&aw, LV_ANIM_REPEAT_INFINITE);
    if (bounce_flag) lv_anim_set_playback_duration(&aw, time_ms);
    lv_anim_start(&aw);

    lv_anim_init(&ah);
    lv_anim_set_var(&ah, obj);
    lv_anim_set_exec_cb(&ah, anim_set_height);
    lv_anim_set_values(&ah, cur_h, th);
    lv_anim_set_duration(&ah, time_ms);
    lv_anim_set_delay(&ah, delay_ms);
    lv_anim_set_path_cb(&ah, path_cb);
    if (repeat_flag) lv_anim_set_repeat_count(&ah, LV_ANIM_REPEAT_INFINITE);
    if (bounce_flag) lv_anim_set_playback_duration(&ah, time_ms);
    lv_anim_start(&ah);

    return 0;
}

/* -------------------------------------------------------------------------
 * Canvas
 * ------------------------------------------------------------------------- */

static int l_canvas(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *scr = (lv_obj_t *)lua_touserdata(L, 1);
    int w  = (int)luaL_checkinteger(L, 2);
    int h  = (int)luaL_checkinteger(L, 3);
    int x  = (int)luaL_optinteger(L, 4, 0);
    int y  = (int)luaL_optinteger(L, 5, 0);

    /* Allocate pixel buffer.  Use LVGL's stride calculation for alignment. */
    uint32_t stride  = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);
    uint32_t buf_len = stride * h;
    void *buf = malloc(buf_len);
    if (!buf) {
        lua_pushnil(L);
        return 1;
    }
    memset(buf, 0, buf_len);

    lv_obj_t *canvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(canvas, buf, w, h, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(canvas, x, y);

    lua_pushlightuserdata(L, canvas);
    return 1;
}

static int l_canvas_clear(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *canvas = (lv_obj_t *)lua_touserdata(L, 1);
    uint32_t rgb565  = (uint32_t)luaL_optinteger(L, 2, 0x0000);

    int32_t w = lv_obj_get_width(canvas);
    int32_t h = lv_obj_get_height(canvas);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_fill_dsc_t dsc;
    lv_draw_fill_dsc_init(&dsc);
    dsc.color = rgb565_to_lvcolor(rgb565);
    dsc.opa   = LV_OPA_COVER;
    lv_area_t area = { 0, 0, (lv_coord_t)(w - 1), (lv_coord_t)(h - 1) };
    lv_draw_fill(&layer, &dsc, &area);

    lv_canvas_finish_layer(canvas, &layer);
    return 0;
}

static int l_canvas_rect(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *canvas = (lv_obj_t *)lua_touserdata(L, 1);
    int x      = (int)luaL_checkinteger(L, 2);
    int y      = (int)luaL_checkinteger(L, 3);
    int w      = (int)luaL_checkinteger(L, 4);
    int h      = (int)luaL_checkinteger(L, 5);
    uint32_t c = (uint32_t)luaL_optinteger(L, 6, 0x39E7);
    int radius = (int)luaL_optinteger(L, 7, 0);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color  = rgb565_to_lvcolor(c);
    dsc.bg_opa    = LV_OPA_COVER;
    dsc.radius    = radius;
    dsc.border_width = 0;
    lv_area_t area = { (lv_coord_t)x, (lv_coord_t)y,
                       (lv_coord_t)(x + w - 1), (lv_coord_t)(y + h - 1) };
    lv_draw_rect(&layer, &dsc, &area);

    lv_canvas_finish_layer(canvas, &layer);
    return 0;
}

static int l_canvas_line(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *canvas = (lv_obj_t *)lua_touserdata(L, 1);
    int x1 = (int)luaL_checkinteger(L, 2);
    int y1 = (int)luaL_checkinteger(L, 3);
    int x2 = (int)luaL_checkinteger(L, 4);
    int y2 = (int)luaL_checkinteger(L, 5);
    uint32_t c = (uint32_t)luaL_optinteger(L, 6, 0xFFFF);
    int width  = (int)luaL_optinteger(L, 7, 1);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = rgb565_to_lvcolor(c);
    dsc.width = width;
    dsc.opa   = LV_OPA_COVER;
    dsc.p1.x  = (lv_value_precise_t)x1;
    dsc.p1.y  = (lv_value_precise_t)y1;
    dsc.p2.x  = (lv_value_precise_t)x2;
    dsc.p2.y  = (lv_value_precise_t)y2;
    lv_draw_line(&layer, &dsc);

    lv_canvas_finish_layer(canvas, &layer);
    return 0;
}

static int l_canvas_circle(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *canvas = (lv_obj_t *)lua_touserdata(L, 1);
    int cx        = (int)luaL_checkinteger(L, 2);
    int cy        = (int)luaL_checkinteger(L, 3);
    int r         = (int)luaL_checkinteger(L, 4);
    uint32_t c    = (uint32_t)luaL_optinteger(L, 5, 0xFFFF);
    int thickness = (int)luaL_optinteger(L, 6, 1);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color       = rgb565_to_lvcolor(c);
    dsc.width       = thickness;
    dsc.opa         = LV_OPA_COVER;
    dsc.center.x    = cx;
    dsc.center.y    = cy;
    dsc.radius      = r;
    dsc.start_angle = 0;
    dsc.end_angle   = 360;
    lv_draw_arc(&layer, &dsc);

    lv_canvas_finish_layer(canvas, &layer);
    return 0;
}

static int l_canvas_arc(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *canvas = (lv_obj_t *)lua_touserdata(L, 1);
    int cx        = (int)luaL_checkinteger(L, 2);
    int cy        = (int)luaL_checkinteger(L, 3);
    int r         = (int)luaL_checkinteger(L, 4);
    int a1        = (int)luaL_checkinteger(L, 5);
    int a2        = (int)luaL_checkinteger(L, 6);
    uint32_t c    = (uint32_t)luaL_optinteger(L, 7, 0xFFFF);
    int thickness = (int)luaL_optinteger(L, 8, 1);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color       = rgb565_to_lvcolor(c);
    dsc.width       = thickness;
    dsc.opa         = LV_OPA_COVER;
    dsc.center.x    = cx;
    dsc.center.y    = cy;
    dsc.radius      = r;
    dsc.start_angle = a1;
    dsc.end_angle   = a2;
    lv_draw_arc(&layer, &dsc);

    lv_canvas_finish_layer(canvas, &layer);
    return 0;
}

/* -------------------------------------------------------------------------
 * ui.canvas_polyline(canvas, points, color, width)
 * points: flat Lua table {x1,y1, x2,y2, ...}
 * ------------------------------------------------------------------------- */
static int l_canvas_polyline(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *canvas = (lv_obj_t *)lua_touserdata(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    uint32_t color = (uint32_t)luaL_optinteger(L, 3, 0xFFFF);
    int width      = (int)luaL_optinteger(L, 4, 1);

    int n    = (int)lua_rawlen(L, 2);
    int npts = n / 2;
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
    dsc.color = rgb565_to_lvcolor(color);
    dsc.width = width;
    dsc.opa   = LV_OPA_COVER;
    for (int i = 0; i < npts - 1; i++) {
        dsc.p1 = pts[i];
        dsc.p2 = pts[i + 1];
        lv_layer_t layer;
        lv_canvas_init_layer(canvas, &layer);
        lv_draw_line(&layer, &dsc);
        lv_canvas_finish_layer(canvas, &layer);
    }

    if (pts != stack_pts) free(pts);
    return 0;
}

/* -------------------------------------------------------------------------
 * ui.canvas_bezier(canvas, x0,y0, cx1,cy1, cx2,cy2, x1,y1, color, width, steps)
 * Cubic Bezier via lv_bezier3 sampling.
 * ------------------------------------------------------------------------- */
static int l_canvas_bezier(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *canvas = (lv_obj_t *)lua_touserdata(L, 1);
    int x0  = (int)luaL_checkinteger(L, 2),  y0  = (int)luaL_checkinteger(L, 3);
    int cx1 = (int)luaL_checkinteger(L, 4),  cy1 = (int)luaL_checkinteger(L, 5);
    int cx2 = (int)luaL_checkinteger(L, 6),  cy2 = (int)luaL_checkinteger(L, 7);
    int x1  = (int)luaL_checkinteger(L, 8),  y1  = (int)luaL_checkinteger(L, 9);
    uint32_t color = (uint32_t)luaL_optinteger(L, 10, 0xFFFF);
    int width      = (int)luaL_optinteger(L, 11, 1);
    int steps      = (int)luaL_optinteger(L, 12, 20);
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
    dsc.color = rgb565_to_lvcolor(color);
    dsc.width = width;
    dsc.opa   = LV_OPA_COVER;
    for (int i = 0; i < steps; i++) {
        dsc.p1 = pts[i];
        dsc.p2 = pts[i + 1];
        lv_layer_t layer;
        lv_canvas_init_layer(canvas, &layer);
        lv_draw_line(&layer, &dsc);
        lv_canvas_finish_layer(canvas, &layer);
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * ui.on_tap(widget, fn)
 * ------------------------------------------------------------------------- */
typedef struct {
    lua_State *L;
    int fn_ref;
} tap_data_t;

static void tap_event_cb(lv_event_t *e)
{
    tap_data_t *d = (tap_data_t *)lv_event_get_user_data(e);
    if (!d) return;
    lua_rawgeti(d->L, LUA_REGISTRYINDEX, d->fn_ref);
    if (lua_pcall(d->L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "[ui.on_tap] error: %s\n", lua_tostring(d->L, -1));
        lua_pop(d->L, 1);
    }
}

static int l_on_tap(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    tap_data_t *d = (tap_data_t *)malloc(sizeof(tap_data_t));
    d->L = L;
    lua_pushvalue(L, 2);
    d->fn_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj, tap_event_cb, LV_EVENT_CLICKED, d);
    return 0;
}

/* -------------------------------------------------------------------------
 * Module registration
 * ------------------------------------------------------------------------- */

static const luaL_Reg ui_funcs[] = {
    { "screen",        l_screen       },
    { "show",          l_show         },
    { "label",         l_label        },
    { "label_set",     l_label_set    },
    { "label_color",   l_label_color  },
    { "arc",           l_arc          },
    { "arc_set",       l_arc_set      },
    { "arc_color",     l_arc_color    },
    { "rect",          l_rect         },
    { "rect_set",      l_rect_set     },
    { "rect_size",     l_rect_size    },
    { "anim_fade",     l_anim_fade    },
    { "anim_move",     l_anim_move    },
    { "anim_stop",     l_anim_stop    },
    { "anim_arc",      l_anim_arc     },
    { "anim_size",     l_anim_size    },
    { "canvas",        l_canvas       },
    { "canvas_clear",  l_canvas_clear },
    { "canvas_rect",   l_canvas_rect  },
    { "canvas_line",   l_canvas_line  },
    { "canvas_circle",   l_canvas_circle   },
    { "canvas_arc",      l_canvas_arc      },
    { "canvas_polyline", l_canvas_polyline },
    { "canvas_bezier",   l_canvas_bezier   },
    { "on_tap",          l_on_tap          },
    { "color",         l_color        },
    { "delete",        l_delete       },
    { NULL, NULL }
};

int luaopen_ui(lua_State *L)
{
    luaL_newlib(L, ui_funcs);
    return 1;
}
