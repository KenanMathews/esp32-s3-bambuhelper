/**
 * BambuHelper SDK Simulator
 *
 * Runs a Lua app script on the desktop using LVGL + SDL2.
 * Simulates the 240x240 round display on BambuHelper hardware.
 *
 * Usage: ./simulator <path/to/app.lua>
 * Default: ../demos/basic-app/app.lua
 */

#include "lvgl/lvgl.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* BambuHelper SDK modules */
#include "sdk_bambu.h"
#include "sdk_ui.h"
#include "sdk_sys.h"

#define BH_SDK_VERSION 1
#define DEFAULT_SCRIPT "../demos/basic-app/app.lua"

/**
 * Read an entire file into a malloc'd buffer.
 * Caller must free() the returned pointer.
 * Returns NULL on error.
 */
static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t nread = fread(buf, 1, len, f);
    fclose(f);
    buf[nread] = '\0';
    return buf;
}

/**
 * Initialize HAL for PC simulator — SDL window, mouse input, cursor.
 */
static void hal_init(void)
{
    /* SDL window — 240x240 round display */
    lv_display_t *disp = lv_sdl_window_create(240, 240);

    /* Mouse input */
    lv_indev_t *mouse_indev = lv_sdl_mouse_create();
    lv_indev_set_group(mouse_indev, lv_group_get_default());
    lv_indev_set_display(mouse_indev, disp);

    /* Mouse cursor image */
    LV_IMAGE_DECLARE(mouse_cursor_icon);
    lv_obj_t *cursor_obj = lv_image_create(lv_screen_active());
    lv_image_set_src(cursor_obj, &mouse_cursor_icon);
    lv_indev_set_cursor(mouse_indev, cursor_obj);
}

/**
 * Main entry point
 */
int main(int argc, char *argv[])
{
    printf("BambuHelper SDK Simulator v%d\n", BH_SDK_VERSION);

    /* Determine script path */
    const char *script_path = (argc > 1) ? argv[1] : DEFAULT_SCRIPT;

    /* Load script from disk */
    char *script = read_file(script_path);
    if (!script) {
        fprintf(stderr, "Error: could not open script '%s'\n", script_path);
        fprintf(stderr, "Usage: simulator <path/to/app.lua>\n");
        return 1;
    }
    printf("Loading script: %s\n", script_path);

    /* Initialize LVGL */
    lv_init();

    /* Initialize HAL (SDL display + mouse) */
    hal_init();
    printf("LVGL initialized (240x240 display)\n");

    /* Create Lua VM */
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        fprintf(stderr, "Error: failed to create Lua state\n");
        free(script);
        return 1;
    }
    luaL_openlibs(L);
    printf("Lua %s initialized\n", LUA_VERSION);

    /*
     * Register BambuHelper SDK modules into package.preload so scripts
     * can use: bambu = require('bambu') etc.
     * We also immediately require them into globals so apps can use
     * bambu.* / ui.* / sys.* without an explicit require.
     */
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");

    lua_pushcfunction(L, luaopen_bambu);
    lua_setfield(L, -2, "bambu");

    lua_pushcfunction(L, luaopen_ui);
    lua_setfield(L, -2, "ui");

    lua_pushcfunction(L, luaopen_sys);
    lua_setfield(L, -2, "sys");

    lua_pop(L, 2); /* pop preload + package */

    /* Pull modules into globals */
    if (luaL_dostring(L, "bambu = require('bambu'); ui = require('ui'); sys = require('sys')") != LUA_OK) {
        fprintf(stderr, "Error loading SDK modules: %s\n", lua_tostring(L, -1));
        lua_close(L);
        free(script);
        return 1;
    }

    printf("SDK modules registered (bambu, ui, sys)\n");

    /* Execute the app script */
    printf("Executing app script...\n");
    if (luaL_dostring(L, script) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        fprintf(stderr, "Lua error: %s\n", err != NULL ? err : "(unknown)");
        lua_pop(L, 1);
        lua_close(L);
        free(script);
        return 1;
    }
    free(script);

    printf("Script executed successfully — entering display loop\n");

    /* Main LVGL event loop */
    while (!sdk_sys_should_exit()) {
        lv_timer_handler();
        usleep(5000); /* 5 ms */
    }
    printf("Simulator exited cleanly.\n");

    /* Unreachable — cleanup shown for completeness */
    lua_close(L);
    return 0;
}
