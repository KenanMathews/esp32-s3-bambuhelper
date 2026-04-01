/**
 * sdk_sys.c — BambuHelper System Module
 *
 * Implements the sys.* Lua API.
 * On desktop: beep is a no-op, http_get and json_parse are stubs,
 * store_* uses in-memory Lua table, time() uses system clock.
 *
 * sys.every(ms, fn)  — repeating timer, matches firmware behaviour
 * sys.after(ms, fn)  — one-shot timer (simulator + SDK extension)
 * sys.exit()         — sets exit flag checked by main loop
 */

#include "sdk_sys.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <lvgl.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Exit flag (checked by main loop via sdk_sys_should_exit)
 * ------------------------------------------------------------------------- */

static volatile int g_should_exit = 0;

int sdk_sys_should_exit(void) { return g_should_exit; }

/* -------------------------------------------------------------------------
 * Tick timer state
 * ------------------------------------------------------------------------- */

static lua_State *g_L           = NULL;
static int        g_tick_ref    = LUA_NOREF;
static lv_timer_t *g_tick_timer = NULL;
static uint32_t   g_last_tick   = 0;

static void tick_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (g_L == NULL || g_tick_ref == LUA_NOREF) return;

    uint32_t now = lv_tick_get();
    int dt = (int)(now - g_last_tick);
    g_last_tick = now;

    lua_rawgeti(g_L, LUA_REGISTRYINDEX, g_tick_ref);
    lua_pushinteger(g_L, dt);
    if (lua_pcall(g_L, 1, 0, 0) != LUA_OK) {
        fprintf(stderr, "[sys.on_tick] error: %s\n", lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
    }
}

/* -------------------------------------------------------------------------
 * sys.on_tick(fn)
 * ------------------------------------------------------------------------- */
static int l_on_tick(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    g_L = L;

    if (g_tick_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, g_tick_ref);
    }
    lua_pushvalue(L, 1);
    g_tick_ref  = luaL_ref(L, LUA_REGISTRYINDEX);
    g_last_tick = lv_tick_get();

    if (g_tick_timer == NULL) {
        g_tick_timer = lv_timer_create(tick_timer_cb, 16, NULL);
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * sys.every / sys.after shared timer infrastructure
 *
 * sys.every(ms, fn)  — repeating (matches firmware)
 * sys.after(ms, fn)  — one-shot
 *
 * Both deduplicate by Lua function reference: re-registering the same fn
 * cancels the previous timer and creates a fresh one.
 * Maximum MAX_TIMER_SLOTS concurrent timers (matches firmware max of 8).
 * ------------------------------------------------------------------------- */
#define MAX_TIMER_SLOTS 8

typedef struct {
    lua_State  *L;
    int         fn_ref;
    lv_timer_t *timer;
    int         active;
    int         one_shot; /* 1 = sys.after, 0 = sys.every */
} timer_slot_t;

static timer_slot_t g_timers[MAX_TIMER_SLOTS];

static void generic_timer_cb(lv_timer_t *timer)
{
    timer_slot_t *s = (timer_slot_t *)lv_timer_get_user_data(timer);
    if (!s || !s->active) return;

    int one_shot = s->one_shot;

    if (one_shot) {
        /* Disarm before calling fn so the slot is free for re-registration
         * from inside the callback. */
        s->active = 0;
        lv_timer_delete(s->timer);
        s->timer = NULL;
    }

    int fn_ref = s->fn_ref;
    if (one_shot) s->fn_ref = LUA_NOREF;

    lua_rawgeti(s->L, LUA_REGISTRYINDEX, fn_ref);

    if (one_shot) {
        luaL_unref(s->L, LUA_REGISTRYINDEX, fn_ref);
    }

    if (lua_pcall(s->L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "[sys.%s] error: %s\n",
                one_shot ? "after" : "every",
                lua_tostring(s->L, -1));
        lua_pop(s->L, 1);
    }
}

/**
 * Cancel any existing slot whose fn matches the function at stack index fn_idx.
 */
static void cancel_matching_timer(lua_State *L, int fn_idx)
{
    for (int i = 0; i < MAX_TIMER_SLOTS; i++) {
        if (!g_timers[i].active) continue;
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_timers[i].fn_ref);
        int same = lua_rawequal(L, fn_idx, -1);
        lua_pop(L, 1);
        if (same) {
            lv_timer_delete(g_timers[i].timer);
            luaL_unref(L, LUA_REGISTRYINDEX, g_timers[i].fn_ref);
            g_timers[i].active = 0;
            g_timers[i].timer  = NULL;
            g_timers[i].fn_ref = LUA_NOREF;
            break;
        }
    }
}

static int register_timer(lua_State *L, int one_shot)
{
    int ms = (int)luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    cancel_matching_timer(L, 2);

    int slot = -1;
    for (int i = 0; i < MAX_TIMER_SLOTS; i++) {
        if (!g_timers[i].active) { slot = i; break; }
    }
    if (slot < 0)
        return luaL_error(L, "sys.%s: max %d concurrent timers reached",
                          one_shot ? "after" : "every", MAX_TIMER_SLOTS);

    lua_pushvalue(L, 2);
    g_timers[slot].fn_ref   = luaL_ref(L, LUA_REGISTRYINDEX);
    g_timers[slot].L        = L;
    g_timers[slot].active   = 1;
    g_timers[slot].one_shot = one_shot;
    g_timers[slot].timer    = lv_timer_create(generic_timer_cb,
                                               (uint32_t)ms,
                                               &g_timers[slot]);
    if (one_shot) {
        lv_timer_set_repeat_count(g_timers[slot].timer, 1);
    }
    return 0;
}

static int l_every(lua_State *L) { return register_timer(L, 0); }
static int l_after(lua_State *L) { return register_timer(L, 1); }

/* -------------------------------------------------------------------------
 * sys.millis()
 * ------------------------------------------------------------------------- */
static int l_millis(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)lv_tick_get());
    return 1;
}

/* -------------------------------------------------------------------------
 * sys.log(msg)
 * ------------------------------------------------------------------------- */
static int l_log(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    printf("[app] %s\n", msg);
    return 0;
}

/* -------------------------------------------------------------------------
 * sys.beep(freq, ms) — no-op on desktop
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
 * ------------------------------------------------------------------------- */
static int l_exit(lua_State *L)
{
    (void)L;
    printf("[sys.exit] exiting simulator\n");
    g_should_exit = 1;
    return 0;
}

/* -------------------------------------------------------------------------
 * sys.sdk_version()
 * ------------------------------------------------------------------------- */
static int l_sdk_version(lua_State *L)
{
    lua_pushinteger(L, 1);
    return 1;
}

/* -------------------------------------------------------------------------
 * sys.time()
 * ------------------------------------------------------------------------- */
static int l_time(lua_State *L)
{
    time_t now  = time(NULL);
    struct tm *t = localtime(&now);

    lua_newtable(L);

    lua_pushinteger(L, (lua_Integer)now);  lua_setfield(L, -2, "epoch");
    lua_pushinteger(L, t->tm_hour);        lua_setfield(L, -2, "hour");
    lua_pushinteger(L, t->tm_min);         lua_setfield(L, -2, "min");
    lua_pushinteger(L, t->tm_sec);         lua_setfield(L, -2, "sec");
    lua_pushinteger(L, t->tm_mday);        lua_setfield(L, -2, "day");
    lua_pushinteger(L, t->tm_mon + 1);     lua_setfield(L, -2, "month");
    lua_pushinteger(L, t->tm_year + 1900); lua_setfield(L, -2, "year");
    lua_pushinteger(L, t->tm_wday);        lua_setfield(L, -2, "wday");
    lua_pushboolean(L, 1);                 lua_setfield(L, -2, "synced");

    return 1;
}

/* -------------------------------------------------------------------------
 * sys.store_set / store_get / store_del  (in-memory, volatile)
 * ------------------------------------------------------------------------- */
static int g_store_ref = LUA_NOREF;

static void ensure_store(lua_State *L)
{
    if (g_store_ref == LUA_NOREF) {
        lua_newtable(L);
        g_store_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
}

static int l_store_set(lua_State *L)
{
    const char *key   = luaL_checkstring(L, 1);
    const char *value = luaL_checkstring(L, 2);
    ensure_store(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_store_ref);
    lua_pushstring(L, value);
    lua_setfield(L, -2, key);
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
    return 1;
}

static int l_store_get(lua_State *L)
{
    const char *key = luaL_checkstring(L, 1);
    const char *def = luaL_optstring(L, 2, "");
    ensure_store(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_store_ref);
    lua_getfield(L, -1, key);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
        lua_pushstring(L, def);
    } else {
        lua_remove(L, -2);
    }
    return 1;
}

static int l_store_del(lua_State *L)
{
    const char *key = luaL_checkstring(L, 1);
    ensure_store(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_store_ref);
    lua_getfield(L, -1, key);
    int existed = !lua_isnil(L, -1);
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_setfield(L, -2, key);
    lua_pop(L, 1);
    lua_pushboolean(L, existed);
    return 1;
}

/* -------------------------------------------------------------------------
 * sys.http_get(url, timeout_ms) — stub
 * ------------------------------------------------------------------------- */
static int l_http_get(lua_State *L)
{
    const char *url = luaL_checkstring(L, 1);
    printf("[sys.http_get] %s — not supported in simulator\n", url);
    lua_pushnil(L);
    lua_pushstring(L, "http_get not supported in simulator");
    return 2;
}

/* -------------------------------------------------------------------------
 * sys.json_parse(str) — stub
 * ------------------------------------------------------------------------- */
static int l_json_parse(lua_State *L)
{
    (void)luaL_checkstring(L, 1);
    lua_pushnil(L);
    lua_pushstring(L, "json_parse not supported in simulator");
    return 2;
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
    { "on_tick",     l_on_tick     },
    { "every",       l_every       },
    { "after",       l_after       },
    { "time",        l_time        },
    { "store_set",   l_store_set   },
    { "store_get",   l_store_get   },
    { "store_del",   l_store_del   },
    { "http_get",    l_http_get    },
    { "json_parse",  l_json_parse  },
    { NULL, NULL }
};

int luaopen_sys(lua_State *L)
{
    luaL_newlib(L, sys_funcs);
    return 1;
}
