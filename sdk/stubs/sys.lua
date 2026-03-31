---@meta

---BambuHelper System Module
---System utilities, timers, storage, networking, and hardware access.
---Most hardware functions (beep) are no-ops in the desktop simulator.
local sys = {}

---Returns milliseconds elapsed since startup.
---In the simulator this uses the process clock; on hardware it uses the RTOS tick.
---@return integer ms Milliseconds since startup
function sys.millis() end

---Print a message to the debug log / serial console.
---In the simulator this writes to stdout with an [app] prefix.
---@param msg string Message to log
function sys.log(msg) end

---Play a beep tone (hardware only — no-op in simulator).
---@param freq integer Frequency in Hz (e.g. 440)
---@param ms integer Duration in milliseconds
function sys.beep(freq, ms) end

---Gracefully exit the current app and return to the launcher.
function sys.exit() end

---Returns the BambuHelper SDK version integer.
---@return integer version SDK version (currently 2)
function sys.sdk_version() end

---Register a function to be called every frame.
---`fn` receives `dt` — milliseconds elapsed since the last call.
---Only one tick function per app; calling sys.on_tick() again replaces the previous.
---Required for sys.every() timers and ui.on_tap() callbacks to fire.
---@param fn fun(dt: integer) Frame callback. dt is ms since last tick.
function sys.on_tick(fn) end

---Schedule a function to be called at most once per `ms` milliseconds.
---Fires from the tick loop — safe to call any ui.* or sys.* inside.
---Requires sys.on_tick() to be registered; timers don't fire in static apps.
---Maximum 8 concurrent timers per app.
---@param ms integer Minimum interval between calls in milliseconds
---@param fn fun() Callback with no arguments
function sys.every(ms, fn) end

---Perform an HTTP GET request. **Blocking** — pauses rendering for the duration.
---Do not call from inside a tick loop without awareness of the frame-drop impact.
---@param url string Full URL to fetch
---@param timeout_ms? integer Request timeout in milliseconds. Default: 8000
---@return string|nil body Response body string, or nil on error
---@return string|nil err Error description, or nil on success
function sys.http_get(url, timeout_ms) end

---Persist a string value under a named key. Stored on LittleFS at /appdata/{key}.
---@param key string Storage key (alphanumeric, no slashes)
---@param value string Value to store
---@return boolean ok true on success
function sys.store_set(key, value) end

---Retrieve a previously stored string value.
---@param key string Storage key
---@param default? string Value to return if key not found. Default: ""
---@return string value Stored value, or default if not found
function sys.store_get(key, default) end

---Delete a stored key.
---@param key string Storage key to delete
---@return boolean ok true if the key existed and was deleted
function sys.store_del(key) end

---Returns the current local time. Requires NTP sync; check `synced` before using.
---`wday` follows C convention: 0 = Sunday, 6 = Saturday.
---@return {epoch: integer, hour: integer, min: integer, sec: integer, day: integer, month: integer, year: integer, wday: integer, synced: boolean} time
function sys.time() end

---Parse a JSON string into a Lua table.
---@param str string JSON string to parse
---@return table|nil result Parsed Lua table, or nil on error
---@return string|nil err Error description, or nil on success
function sys.json_parse(str) end

return sys
