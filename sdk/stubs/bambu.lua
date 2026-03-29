---@meta

---BambuHelper Printer Data Module
---Provides live data from the connected Bambu Lab printer.
---In the simulator these values are hardcoded mocks (see sdk_impl/sdk_bambu.c).
local bambu = {}

---Returns the current gcode state string.
---@return string state One of: "PRINTING", "IDLE", "PAUSE", "FAILED", "FINISH", "PREPARE"
function bambu.state() end

---Returns the current print progress as an integer percentage.
---@return integer progress Value from 0 to 100
function bambu.progress() end

---Returns the current nozzle temperature in degrees Celsius.
---@return number temp_c Current nozzle temperature
function bambu.nozzle_temp() end

---Returns the current bed temperature in degrees Celsius.
---@return number temp_c Current bed temperature
function bambu.bed_temp() end

---Returns the nozzle target temperature in degrees Celsius.
---@return number temp_c Nozzle setpoint
function bambu.nozzle_target() end

---Returns the bed target temperature in degrees Celsius.
---@return number temp_c Bed setpoint
function bambu.bed_target() end

---Returns the estimated minutes remaining in the current print.
---@return integer minutes Remaining time (0 when not printing)
function bambu.remaining_mins() end

---Returns the part cooling fan speed as a percentage.
---@return integer pct Fan speed 0–100
function bambu.fan_part() end

---Returns the auxiliary fan speed as a percentage.
---@return integer pct Fan speed 0–100
function bambu.fan_aux() end

---Returns the user-configured printer name.
---@return string name Printer display name
function bambu.printer_name() end

---Returns whether the printer is currently reachable over the network.
---@return boolean connected true if connected
function bambu.connected() end

---Returns whether a print job is actively running (not paused or idle).
---@return boolean printing true if printing
function bambu.printing() end

return bambu
