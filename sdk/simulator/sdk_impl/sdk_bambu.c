/**
 * sdk_bambu.c — BambuHelper Printer Data Module (mock implementation)
 *
 * Exposes the bambu.* API to Lua with hardcoded values that simulate a
 * printer actively printing.
 *
 * Edit the return values below to simulate different printer states.
 * For a more dynamic simulation you can also edit mock/printer_state.json
 * (currently not parsed at runtime — see that file for details).
 */

#include "sdk_bambu.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* -------------------------------------------------------------------------
 * Mock data — edit these to simulate different printer states.
 * ------------------------------------------------------------------------- */

static int l_state(lua_State *L)
{
    lua_pushstring(L, "PRINTING");
    return 1;
}

static int l_progress(lua_State *L)
{
    lua_pushinteger(L, 42);
    return 1;
}

static int l_nozzle_temp(lua_State *L)
{
    lua_pushnumber(L, 215.0);
    return 1;
}

static int l_bed_temp(lua_State *L)
{
    lua_pushnumber(L, 55.0);
    return 1;
}

static int l_nozzle_target(lua_State *L)
{
    lua_pushnumber(L, 220.0);
    return 1;
}

static int l_bed_target(lua_State *L)
{
    lua_pushnumber(L, 60.0);
    return 1;
}

static int l_remaining_mins(lua_State *L)
{
    lua_pushinteger(L, 28);
    return 1;
}

static int l_fan_part(lua_State *L)
{
    lua_pushinteger(L, 80);
    return 1;
}

static int l_fan_aux(lua_State *L)
{
    lua_pushinteger(L, 30);
    return 1;
}

static int l_printer_name(lua_State *L)
{
    lua_pushstring(L, "My Bambu X1C");
    return 1;
}

static int l_connected(lua_State *L)
{
    lua_pushboolean(L, 1);
    return 1;
}

static int l_printing(lua_State *L)
{
    lua_pushboolean(L, 1);
    return 1;
}

/* -------------------------------------------------------------------------
 * Module registration
 * ------------------------------------------------------------------------- */

static const luaL_Reg bambu_funcs[] = {
    { "state",          l_state         },
    { "progress",       l_progress      },
    { "nozzle_temp",    l_nozzle_temp   },
    { "bed_temp",       l_bed_temp      },
    { "nozzle_target",  l_nozzle_target },
    { "bed_target",     l_bed_target    },
    { "remaining_mins", l_remaining_mins },
    { "fan_part",       l_fan_part      },
    { "fan_aux",        l_fan_aux       },
    { "printer_name",   l_printer_name  },
    { "connected",      l_connected     },
    { "printing",       l_printing      },
    { NULL, NULL }
};

int luaopen_bambu(lua_State *L)
{
    luaL_newlib(L, bambu_funcs);
    return 1;
}
