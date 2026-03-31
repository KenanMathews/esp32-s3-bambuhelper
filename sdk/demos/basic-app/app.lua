-- @name        Basic Status
-- @version     1.0
-- @sdk_min     1
-- @author      BambuHelper Team
-- @description Shows printer name, progress arc, nozzle and bed temps
-- @color       0x07E0
-- @category    display

-- Create the main screen
local scr = ui.screen()

-- Printer name at top
ui.label(scr, bambu.printer_name(), {
    align = "top_mid", y = 18,
    color = 0xFFFF, font = 16
})

-- Large progress arc in center
ui.arc(scr, {
    cx = 120, cy = 115,
    value = bambu.progress(),
    color = 0x07E0,
    size = 90
})

-- Progress percent inside arc
ui.label(scr, bambu.progress() .. "%", {
    align = "center", y = -8,
    color = 0xFFFF, font = 20
})

-- State label below arc
local state = bambu.state()
local state_color = 0x07E0
if state == "PAUSE" then state_color = 0xFFE0
elseif state == "FAILED" then state_color = 0xF800
elseif state == "IDLE" then state_color = 0xC618
end
ui.label(scr, state, {
    align = "center", y = 30,
    color = state_color, font = 14
})

-- Nozzle temp (bottom left area)
ui.label(scr, string.format("%.0f\xc2\xb0C", bambu.nozzle_temp()), {
    align = "bottom_mid", x = -35, y = -28,
    color = 0xFBE0, font = 16
})

-- Bed temp (bottom right area)
ui.label(scr, string.format("%.0f\xc2\xb0C", bambu.bed_temp()), {
    align = "bottom_mid", x = 35, y = -28,
    color = 0x34DF, font = 16
})


sys.log("basic-app loaded — " .. bambu.printer_name())
