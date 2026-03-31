---@meta

---BambuHelper Printer Data Module
---Provides live data from the connected Bambu Lab printer.
---In the simulator these values are hardcoded mocks.
local bambu = {}

-- ---------------------------------------------------------------------------
--  Print state
-- ---------------------------------------------------------------------------

---Returns the current gcode state string.
---@return string state One of: "PRINTING", "IDLE", "PAUSE", "FAILED", "FINISH", "PREPARE"
function bambu.state() end

---Returns the current print progress as an integer percentage.
---@return integer progress Value from 0 to 100
function bambu.progress() end

---Returns whether the printer is currently reachable over the network.
---@return boolean connected
function bambu.connected() end

---Returns whether a print job is actively running (not paused or idle).
---@return boolean printing
function bambu.printing() end

---Returns the current layer number being printed.
---@return integer layer Current layer (0 when not printing)
function bambu.layer() end

---Returns the total number of layers in the current job.
---@return integer layers Total layers (0 when not printing)
function bambu.total_layers() end

---Returns the name of the current print job file.
---@return string name Job filename, or "" when idle
function bambu.job_name() end

---Returns the current print speed level.
---@return integer level 0=Silent, 1=Standard, 2=Sport, 3=Ludicrous
function bambu.speed() end

---Returns the estimated minutes remaining in the current print.
---@return integer minutes Remaining time (0 when not printing)
function bambu.remaining_mins() end

-- ---------------------------------------------------------------------------
--  Temperatures
-- ---------------------------------------------------------------------------

---Returns the current nozzle temperature in degrees Celsius.
---@return number temp_c
function bambu.nozzle_temp() end

---Returns the nozzle target (setpoint) temperature in degrees Celsius.
---@return number temp_c
function bambu.nozzle_target() end

---Returns the current bed temperature in degrees Celsius.
---@return number temp_c
function bambu.bed_temp() end

---Returns the bed target (setpoint) temperature in degrees Celsius.
---@return number temp_c
function bambu.bed_target() end

---Returns the current chamber temperature in degrees Celsius.
---@return number temp_c
function bambu.chamber_temp() end

-- ---------------------------------------------------------------------------
--  Fans
-- ---------------------------------------------------------------------------

---Returns the part cooling fan speed as a percentage.
---@return integer pct 0–100
function bambu.fan_part() end

---Returns the auxiliary fan speed as a percentage.
---@return integer pct 0–100
function bambu.fan_aux() end

---Returns the chamber fan speed as a percentage.
---@return integer pct 0–100
function bambu.fan_chamber() end

-- ---------------------------------------------------------------------------
--  Device
-- ---------------------------------------------------------------------------

---Returns the user-configured printer name.
---@return string name
function bambu.printer_name() end

-- ---------------------------------------------------------------------------
--  AMS (filament system)
-- ---------------------------------------------------------------------------

---Returns the colour of an AMS tray as an RGB565 integer.
---Returns 0 (not nil) if the tray is absent — always check `> 0` before using.
---@param tray_index integer Tray index 0–15
---@return integer rgb565 Tray colour, or 0 if not present
function bambu.ams_color(tray_index) end

---Returns the filament type string for an AMS tray (e.g. "PLA", "PETG").
---Returns "" if the tray is absent.
---@param tray_index integer Tray index 0–15
---@return string filament_type
function bambu.ams_type(tray_index) end

---Returns the index of the currently active AMS tray.
---@return integer tray_index Active tray 0–15, or -1 if no AMS tray is active
function bambu.ams_active() end

return bambu
