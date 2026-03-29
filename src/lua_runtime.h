#ifndef LUA_RUNTIME_H
#define LUA_RUNTIME_H
#include <stdint.h>
#include <stddef.h>

// Returns true if Lua VM is available and initialized
bool luaRuntimeInit();

// Run a script. If the script calls sys.on_tick(fn), the runtime enters a
// tick loop calling fn(dt_ms) each iteration (pumping LVGL between calls)
// until sys.exit() is called or luaRuntimeStop() is called externally.
bool luaRuntimeRun(const char* script, size_t len, const char* name);

// Tick the running app one frame — call from main loop when SCREEN_APP is active.
// Returns false when the app has finished and screen should be released.
bool luaRuntimeTick();

// Stop currently running script (call from long-press handler)
void luaRuntimeStop();

// True while a tick-loop app is active
bool luaRuntimeRunning();

const char* luaRuntimeLastError();

#endif // LUA_RUNTIME_H
