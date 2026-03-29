#ifndef LUA_RUNTIME_H
#define LUA_RUNTIME_H
#include <stdint.h>
#include <stddef.h>

// Returns true if Lua VM is available and initialized
bool luaRuntimeInit();

// Run a null-terminated Lua script. Returns true on success.
// Script runs until completion or luaRuntimeStop() is called.
bool luaRuntimeRun(const char* script, size_t len, const char* name);

// Stop currently running script (call from long-press handler)
void luaRuntimeStop();

bool luaRuntimeRunning();

const char* luaRuntimeLastError();

#endif // LUA_RUNTIME_H
