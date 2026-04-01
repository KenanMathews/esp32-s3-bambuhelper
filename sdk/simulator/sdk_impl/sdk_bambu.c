/**
 * sdk_bambu.c — BambuHelper Printer Data Module (mock implementation)
 *
 * Exposes the bambu.* API to Lua with hardcoded values that simulate a
 * printer actively printing at layer 87 of 250.
 *
 * Edit the return values below to simulate different printer states.
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
    lua_pushstring(L, "IDLE");
    return 1;
}

static int l_print_stage(lua_State *L)
{
    /* 0 = normal printing; change to test sub-states:
       1=bed level, 2=heating, 8=flow cal, 13=homing, 19=flow cal alt */
    lua_pushinteger(L, 0);
    return 1;
}

static int l_progress(lua_State *L)
{
    lua_pushinteger(L, 42);
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

static int l_layer(lua_State *L)
{
    lua_pushinteger(L, 87);
    return 1;
}

static int l_total_layers(lua_State *L)
{
    lua_pushinteger(L, 250);
    return 1;
}

static int l_job_name(lua_State *L)
{
    lua_pushstring(L, "benchy.3mf");
    return 1;
}

static int l_speed(lua_State *L)
{
    /* 0=Silent, 1=Standard, 2=Sport, 3=Ludicrous */
    lua_pushinteger(L, 1);
    return 1;
}

static int l_remaining_mins(lua_State *L)
{
    lua_pushinteger(L, 28);
    return 1;
}

static int l_nozzle_temp(lua_State *L)
{
    lua_pushnumber(L, 215.0);
    return 1;
}

static int l_nozzle_target(lua_State *L)
{
    lua_pushnumber(L, 220.0);
    return 1;
}

static int l_bed_temp(lua_State *L)
{
    lua_pushnumber(L, 55.0);
    return 1;
}

static int l_bed_target(lua_State *L)
{
    lua_pushnumber(L, 60.0);
    return 1;
}

static int l_chamber_temp(lua_State *L)
{
    lua_pushnumber(L, 28.0);
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

static int l_fan_chamber(lua_State *L)
{
    lua_pushinteger(L, 0);
    return 1;
}

static int l_printer_name(lua_State *L)
{
    lua_pushstring(L, "My Bambu X1C");
    return 1;
}

/* -------------------------------------------------------------------------
 * AMS mock — 4 trays loaded, tray 0 active
 * ------------------------------------------------------------------------- */

/* RGB565 mock colours for trays 0-3 */
static const uint32_t ams_colors[4] = {
    0xF800, /* red   */
    0x07E0, /* green */
    0x001F, /* blue  */
    0xFFE0, /* yellow */
};

static const char *ams_types[4] = { "PLA", "PETG", "PLA", "ABS" };

static int l_ams_color(lua_State *L)
{
    int idx = (int)luaL_checkinteger(L, 1);
    if (idx >= 0 && idx < 4) {
        lua_pushinteger(L, (lua_Integer)ams_colors[idx]);
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

static int l_ams_type(lua_State *L)
{
    int idx = (int)luaL_checkinteger(L, 1);
    if (idx >= 0 && idx < 4) {
        lua_pushstring(L, ams_types[idx]);
    } else {
        lua_pushstring(L, "");
    }
    return 1;
}

static int l_ams_active(lua_State *L)
{
    lua_pushinteger(L, 0); /* tray 0 is active */
    return 1;
}

/* -------------------------------------------------------------------------
 * Module registration
 * ------------------------------------------------------------------------- */

static const luaL_Reg bambu_funcs[] = {
    { "state",          l_state          },
    { "print_stage",    l_print_stage    },
    { "progress",       l_progress       },
    { "connected",      l_connected      },
    { "printing",       l_printing       },
    { "layer",          l_layer          },
    { "total_layers",   l_total_layers   },
    { "job_name",       l_job_name       },
    { "speed",          l_speed          },
    { "remaining_mins", l_remaining_mins },
    { "nozzle_temp",    l_nozzle_temp    },
    { "nozzle_target",  l_nozzle_target  },
    { "bed_temp",       l_bed_temp       },
    { "bed_target",     l_bed_target     },
    { "chamber_temp",   l_chamber_temp   },
    { "fan_part",       l_fan_part       },
    { "fan_aux",        l_fan_aux        },
    { "fan_chamber",    l_fan_chamber    },
    { "printer_name",   l_printer_name   },
    { "ams_color",      l_ams_color      },
    { "ams_type",       l_ams_type       },
    { "ams_active",     l_ams_active     },
    { NULL, NULL }
};

int luaopen_bambu(lua_State *L)
{
    luaL_newlib(L, bambu_funcs);
    return 1;
}
