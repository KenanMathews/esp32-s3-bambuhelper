/**
 * sdk_sys.c — BambuHelper System Module
 *
 * Implements the sys.* Lua API.
 * On desktop most hardware functions (beep) are no-ops that print a note.
 */

#include "sdk_sys.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <time.h>

/* -------------------------------------------------------------------------
 * sys.millis()
 *
 * Returns approximate milliseconds since the process started.
 * Uses POSIX clock() for portability (no SDL dependency in sys module).
 * ------------------------------------------------------------------------- */
static int l_millis(lua_State *L)
{
    int ms = (int)((clock() * 1000) / CLOCKS_PER_SEC);
    lua_pushinteger(L, ms);
    return 1;
}

/* -------------------------------------------------------------------------
 * sys.log(msg)
 *
 * Print a message to stdout tagged as coming from the app script.
 * ------------------------------------------------------------------------- */
static int l_log(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    printf("[app] %s\n", msg);
    return 0;
}

/* -------------------------------------------------------------------------
 * sys.beep(freq, ms)
 *
 * No-op on desktop — prints a note so you can verify calls are reaching C.
 * ------------------------------------------------------------------------- */
static int l_beep(lua_State *L)
{
    int freq = (int)luaL_optinteger(L, 1, 440);
    int ms   = (int)luaL_optinteger(L, 2, 100);
    printf("[sys.beep] freq=%d ms=%d (no-op on desktop)\n", freq, ms);
    return 0;
}

/* -------------------------------------------------------------------------
 * sys.exit()
 *
 * Signals the app to stop. In the simulator we just log and return —
 * the LVGL loop will keep running so you can inspect the last frame.
 * On hardware this would trigger a clean shutdown.
 * ------------------------------------------------------------------------- */
static int l_exit(lua_State *L)
{
    (void)L;
    printf("[sys.exit] called — simulator will keep running\n");
    return 0;
}

/* -------------------------------------------------------------------------
 * sys.sdk_version()
 *
 * Returns the integer SDK version. Apps can gate features on this.
 * ------------------------------------------------------------------------- */
static int l_sdk_version(lua_State *L)
{
    lua_pushinteger(L, 1);
    return 1;
}

/* -------------------------------------------------------------------------
 * Module registration
 * ------------------------------------------------------------------------- */

static const luaL_Reg sys_funcs[] = {
    { "millis",      l_millis      },
    { "log",         l_log         },
    { "beep",        l_beep        },
    { "exit",        l_exit        },
    { "sdk_version", l_sdk_version },
    { NULL, NULL }
};

int luaopen_sys(lua_State *L)
{
    luaL_newlib(L, sys_funcs);
    return 1;
}
