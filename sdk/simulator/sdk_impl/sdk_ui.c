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

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

/**
 * Convert an RGB565 packed integer to lv_color_t.
 *
 * RGB565 bit layout: RRRRR GGGGGG BBBBB
 * We expand each channel to 8 bits by repeating the MSBs.
 */
static lv_color_t rgb565_to_lvcolor(uint32_t rgb565)
{
    uint8_t r5 = (rgb565 >> 11) & 0x1F;
    uint8_t g6 = (rgb565 >>  5) & 0x3F;
    uint8_t b5 = (rgb565      ) & 0x1F;

    /* Expand to 8-bit: multiply to fill range */
    uint8_t r8 = (r5 << 3) | (r5 >> 2);
    uint8_t g8 = (g6 << 2) | (g6 >> 4);
    uint8_t b8 = (b5 << 3) | (b5 >> 2);

    return lv_color_make(r8, g8, b8);
}

/**
 * Map an alignment string to an lv_align_t constant.
 * Defaults to LV_ALIGN_CENTER for unrecognised values.
 */
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
    return LV_ALIGN_CENTER;
}

/**
 * Map a font size integer to the corresponding Montserrat font pointer.
 * Falls back to montserrat_14 for unrecognised sizes.
 */
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

/* -------------------------------------------------------------------------
 * ui.screen()
 *
 * Creates a new LVGL screen object (not yet displayed).
 * Returns: lightuserdata (lv_obj_t*)
 * ------------------------------------------------------------------------- */
static int l_screen(lua_State *L)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lua_pushlightuserdata(L, scr);
    return 1;
}

/* -------------------------------------------------------------------------
 * ui.label(scr, text, opts)
 *
 * Args:
 *   scr  — lightuserdata screen from ui.screen()
 *   text — string to display
 *   opts — optional table:
 *     align  string   alignment string (see str_to_align)
 *     x      integer  x offset from alignment point
 *     y      integer  y offset from alignment point
 *     color  integer  RGB565 text colour
 *     font   integer  font size (14 / 16 / 20 / 28 / 40)
 *
 * Returns: lightuserdata (lv_obj_t*)
 * ------------------------------------------------------------------------- */
static int l_label(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *scr = (lv_obj_t *)lua_touserdata(L, 1);
    const char *text = luaL_checkstring(L, 2);

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, text);

    /* Parse opts table if provided */
    if (lua_istable(L, 3)) {
        /* align + offsets */
        lv_align_t align = LV_ALIGN_CENTER;
        int ox = 0, oy = 0;

        lua_getfield(L, 3, "align");
        if (lua_isstring(L, -1)) {
            align = str_to_align(lua_tostring(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "x");
        if (lua_isnumber(L, -1)) ox = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 3, "y");
        if (lua_isnumber(L, -1)) oy = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lv_obj_align(lbl, align, ox, oy);

        /* color */
        lua_getfield(L, 3, "color");
        if (lua_isnumber(L, -1)) {
            uint32_t rgb565 = (uint32_t)lua_tointeger(L, -1);
            lv_obj_set_style_text_color(lbl, rgb565_to_lvcolor(rgb565), 0);
        }
        lua_pop(L, 1);

        /* font */
        lua_getfield(L, 3, "font");
        if (lua_isnumber(L, -1)) {
            int size = (int)lua_tointeger(L, -1);
            lv_obj_set_style_text_font(lbl, size_to_font(size), 0);
        }
        lua_pop(L, 1);
    }

    lua_pushlightuserdata(L, lbl);
    return 1;
}

/* -------------------------------------------------------------------------
 * ui.arc(scr, opts)
 *
 * Args:
 *   scr  — lightuserdata screen from ui.screen()
 *   opts — table:
 *     cx        integer  centre x position
 *     cy        integer  centre y position
 *     value     integer  current value (default 0)
 *     min       integer  range minimum (default 0)
 *     max       integer  range maximum (default 100)
 *     color     integer  RGB565 indicator colour
 *     size      integer  outer diameter in pixels (default 80)
 *     thickness integer  arc line width (default 6)
 *
 * Returns: lightuserdata (lv_obj_t*)
 * ------------------------------------------------------------------------- */
static int l_arc(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *scr = (lv_obj_t *)lua_touserdata(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    /* Defaults */
    int cx = 120, cy = 120;
    int value = 0, min = 0, max = 100;
    int size = 80, thickness = 6;
    uint32_t color_rgb565 = 0x07E0; /* green */
    int has_color = 0;

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
    if (lua_isnumber(L, -1)) {
        color_rgb565 = (uint32_t)lua_tointeger(L, -1);
        has_color = 1;
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "size");
    if (lua_isnumber(L, -1)) size = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "thickness");
    if (lua_isnumber(L, -1)) thickness = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    /* Create arc */
    lv_obj_t *arc = lv_arc_create(scr);
    lv_obj_set_size(arc, size, size);

    /* Horseshoe shape matching device (gap at bottom) */
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_arc_set_rotation(arc, 0);

    lv_arc_set_range(arc, min, max);
    lv_arc_set_value(arc, value);

    /* Hide the interactive knob */
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    /* Indicator colour */
    if (has_color) {
        lv_obj_set_style_arc_color(arc, rgb565_to_lvcolor(color_rgb565), LV_PART_INDICATOR);
    }

    /* Arc line width */
    lv_obj_set_style_arc_width(arc, thickness, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, thickness, LV_PART_INDICATOR);

    /* Position by centre point */
    lv_obj_set_pos(arc, cx - size / 2, cy - size / 2);

    lua_pushlightuserdata(L, arc);
    return 1;
}

/* -------------------------------------------------------------------------
 * ui.rect(scr, opts)
 *
 * Args:
 *   scr  — lightuserdata screen from ui.screen()
 *   opts — table:
 *     x      integer  left edge
 *     y      integer  top edge
 *     w      integer  width
 *     h      integer  height
 *     color  integer  RGB565 fill colour
 *     radius integer  corner radius (default 4)
 *
 * Returns: lightuserdata (lv_obj_t*)
 * ------------------------------------------------------------------------- */
static int l_rect(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *scr = (lv_obj_t *)lua_touserdata(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    int x = 0, y = 0, w = 40, h = 40, radius = 4;
    uint32_t color_rgb565 = 0x39E7; /* mid-grey */

    lua_getfield(L, 2, "x");
    if (lua_isnumber(L, -1)) x = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "y");
    if (lua_isnumber(L, -1)) y = (int)lua_tointeger(L, -1);
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

    lv_obj_t *rect = lv_obj_create(scr);
    lv_obj_set_size(rect, w, h);
    lv_obj_set_pos(rect, x, y);
    lv_obj_set_style_bg_color(rect, rgb565_to_lvcolor(color_rgb565), 0);
    lv_obj_set_style_radius(rect, radius, 0);
    lv_obj_set_style_border_width(rect, 0, 0);
    lv_obj_set_style_pad_all(rect, 0, 0);

    lua_pushlightuserdata(L, rect);
    return 1;
}

/* -------------------------------------------------------------------------
 * ui.show(scr)
 *
 * Makes the given screen visible.
 * ------------------------------------------------------------------------- */
static int l_show(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    lv_obj_t *scr = (lv_obj_t *)lua_touserdata(L, 1);
    lv_screen_load(scr);
    return 0;
}

/* -------------------------------------------------------------------------
 * Module registration
 * ------------------------------------------------------------------------- */

static const luaL_Reg ui_funcs[] = {
    { "screen", l_screen },
    { "label",  l_label  },
    { "arc",    l_arc    },
    { "rect",   l_rect   },
    { "show",   l_show   },
    { NULL, NULL }
};

int luaopen_ui(lua_State *L)
{
    luaL_newlib(L, ui_funcs);
    return 1;
}
