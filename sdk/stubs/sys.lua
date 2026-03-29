---@meta

---BambuHelper System Module
---System utilities and hardware access.
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
---In the simulator this is a no-op (logs a message) so you can inspect the last frame.
function sys.exit() end

---Returns the BambuHelper SDK version integer.
---Use this to guard features that require a minimum SDK version.
---@return integer version SDK version (currently 1)
function sys.sdk_version() end

return sys
