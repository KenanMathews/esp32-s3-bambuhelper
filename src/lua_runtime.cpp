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
//  PSRAM allocator and VM state — compiled only with Lua
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
//  Shared state (always compiled so headers stay consistent)
// ---------------------------------------------------------------------------
static char s_lastError[128] = "";

// ---------------------------------------------------------------------------
//  luaRuntimeInit
// ---------------------------------------------------------------------------
bool luaRuntimeInit() {
#ifndef LUA_AVAILABLE
    Serial.println("LuaRuntime: LUA_AVAILABLE not defined — Lua disabled");
    return false;
#else
    if (L) {
        lua_close(L);
        L = nullptr;
    }
    L = lua_newstate(lua_psram_alloc, nullptr);
    if (!L) {
        Serial.println("LuaRuntime: lua_newstate failed");
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

    return true;
#endif
}

// ---------------------------------------------------------------------------
//  luaRuntimeStop — reset VM (close + reopen)
// ---------------------------------------------------------------------------
void luaRuntimeStop() {
#ifdef LUA_AVAILABLE
    if (L) {
        lua_close(L);
        L = nullptr;
    }
    luaRuntimeInit();
#endif
}

// ---------------------------------------------------------------------------
//  luaRuntimeRunning — async execution is future work
// ---------------------------------------------------------------------------
bool luaRuntimeRunning() {
    return false;
}

// ---------------------------------------------------------------------------
//  luaRuntimeLastError
// ---------------------------------------------------------------------------
const char* luaRuntimeLastError() {
    return s_lastError;
}
