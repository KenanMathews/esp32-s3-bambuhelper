#ifndef SDK_API_H
#define SDK_API_H

#define SDK_VERSION 2

#ifdef LUA_AVAILABLE
extern "C" {
#include "lua.h"
}

// Register all SDK modules into the Lua state
void sdkApiRegister(lua_State* L);

// Called by lua_runtime after script runs — returns LUA_NOREF if no tick fn
int  sdkGetTickRef(lua_State* L);

// sys.exit() flag accessors
bool sdkExitRequested();
void sdkClearExitFlag();

#endif // LUA_AVAILABLE
#endif // SDK_API_H
