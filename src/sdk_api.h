#ifndef SDK_API_H
#define SDK_API_H

#define SDK_VERSION 1

#ifdef LUA_AVAILABLE
extern "C" {
#include "lua.h"
}
void sdkApiRegister(lua_State* L);
#endif

#endif // SDK_API_H
