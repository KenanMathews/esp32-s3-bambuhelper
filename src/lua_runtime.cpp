#include "lua_runtime.h"
#include "sdk_api.h"
#include "config.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Lua includes — only when library is present
// ---------------------------------------------------------------------------
#ifdef LUA_AVAILABLE
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#endif

// ---------------------------------------------------------------------------
//  PSRAM allocator and VM state
// ---------------------------------------------------------------------------
#ifdef LUA_AVAILABLE
static void* lua_psram_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    (void)ud; (void)osize;
    if (nsize == 0) { heap_caps_free(ptr); return nullptr; }
    return heap_caps_realloc(ptr, nsize, MALLOC_CAP_SPIRAM);
}
static lua_State* L = nullptr;
#endif

// ---------------------------------------------------------------------------
//  Shared state
// ---------------------------------------------------------------------------
static char  s_lastError[128] = "";
static bool  s_running        = false;   // tick loop active
static int   s_tickRef        = LUA_NOREF;
static unsigned long s_lastTickMs = 0;

// ---------------------------------------------------------------------------
//  luaRuntimeInit
// ---------------------------------------------------------------------------
bool luaRuntimeInit() {
#ifndef LUA_AVAILABLE
    Serial.println("LuaRuntime: LUA_AVAILABLE not defined — Lua disabled");
    return false;
#else
    s_running  = false;
    s_tickRef  = LUA_NOREF;
    if (L) { lua_close(L); L = nullptr; }

    L = lua_newstate(lua_psram_alloc, nullptr);
    if (!L) {
        strlcpy(s_lastError, "lua_newstate failed", sizeof(s_lastError));
        return false;
    }
    luaL_openlibs(L);
    sdkApiRegister(L);
    Serial.println("LuaRuntime: VM ready");
    return true;
#endif
}

// ---------------------------------------------------------------------------
//  luaRuntimeRun
//  Loads and executes the script.  If the script calls sys.on_tick(fn),
//  s_tickRef is set and luaRuntimeRunning() returns true — the caller must
//  pump luaRuntimeTick() every loop iteration.
// ---------------------------------------------------------------------------
bool luaRuntimeRun(const char* script, size_t len, const char* name) {
#ifndef LUA_AVAILABLE
    (void)script; (void)len; (void)name;
    return false;
#else
    if (!L) {
        strlcpy(s_lastError, "VM not initialized", sizeof(s_lastError));
        return false;
    }
    s_lastError[0] = '\0';
    s_tickRef      = LUA_NOREF;
    s_running      = false;

    int loadResult = luaL_loadbuffer(L, script, len, name ? name : "?");
    if (loadResult != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        strlcpy(s_lastError, err ? err : "load error", sizeof(s_lastError));
        lua_pop(L, 1);
        Serial.printf("LuaRuntime: load error: %s\n", s_lastError);
        return false;
    }

    int callResult = lua_pcall(L, 0, 0, 0);
    if (callResult != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        strlcpy(s_lastError, err ? err : "runtime error", sizeof(s_lastError));
        lua_pop(L, 1);
        Serial.printf("LuaRuntime: runtime error: %s\n", s_lastError);
        return false;
    }

    // If script registered a tick function, enter tick-loop mode
    s_tickRef  = sdkGetTickRef(L);
    s_running  = (s_tickRef != LUA_NOREF);
    s_lastTickMs = millis();
    return true;
#endif
}

// ---------------------------------------------------------------------------
//  luaRuntimeTick — called every loop() when SCREEN_APP is active
//  Returns false when the app has finished (caller should release screen).
// ---------------------------------------------------------------------------
bool luaRuntimeTick() {
#ifndef LUA_AVAILABLE
    return false;
#else
    if (!s_running || !L || s_tickRef == LUA_NOREF) return false;

    unsigned long now = millis();
    unsigned long dt  = now - s_lastTickMs;
    s_lastTickMs = now;

    // Check sys.exit() flag set during a previous tick
    if (sdkExitRequested()) {
        sdkClearExitFlag();
        s_running = false;
        luaL_unref(L, LUA_REGISTRYINDEX, s_tickRef);
        s_tickRef = LUA_NOREF;
        return false;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, s_tickRef);
    lua_pushinteger(L, (long long)dt);
    int result = lua_pcall(L, 1, 0, 0);
    if (result != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        strlcpy(s_lastError, err ? err : "tick error", sizeof(s_lastError));
        lua_pop(L, 1);
        Serial.printf("LuaRuntime: tick error: %s\n", s_lastError);
        s_running = false;
        luaL_unref(L, LUA_REGISTRYINDEX, s_tickRef);
        s_tickRef = LUA_NOREF;
        return false;
    }

    // Check exit flag again — may have been set inside the tick
    if (sdkExitRequested()) {
        sdkClearExitFlag();
        s_running = false;
        luaL_unref(L, LUA_REGISTRYINDEX, s_tickRef);
        s_tickRef = LUA_NOREF;
        return false;
    }

    return true;
#endif
}

// ---------------------------------------------------------------------------
//  luaRuntimeStop
// ---------------------------------------------------------------------------
void luaRuntimeStop() {
#ifdef LUA_AVAILABLE
    s_running = false;
    if (L && s_tickRef != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, s_tickRef);
        s_tickRef = LUA_NOREF;
    }
    if (L) { lua_close(L); L = nullptr; }
    luaRuntimeInit();
#endif
}

bool luaRuntimeRunning() { return s_running; }

const char* luaRuntimeLastError() { return s_lastError; }
