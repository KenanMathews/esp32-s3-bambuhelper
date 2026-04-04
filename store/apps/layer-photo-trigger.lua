-- @name Layer Photo Trigger
-- @color 0xF81F
-- @sdk_min 1
-- @version 1.0
-- @author BambuHelper
-- @category monitor
-- @description Shows current layer; flashes when layer changes (syncs with the Expo camera app).

local C_BG    = 0x0000
local C_WHT   = 0xFFFF
local C_GREY  = 0x8410
local C_GRN   = 0x07E0
local C_CYAN  = 0x07FF
local C_ORNG  = 0xFD20
local C_MAG   = 0xF81F

local CX, CY = 120, 120

local scr    = ui.screen()
local canvas = ui.canvas(scr, 240, 240, 0, 0)

-- Labels
local lbl_layer   = ui.label(scr, "--",        {align="center",     y=-18, font=40, color=C_WHT})
local lbl_total   = ui.label(scr, "of --",     {align="center",     y=28,  font=16, color=C_GREY})
local lbl_status  = ui.label(scr, "WAITING",   {align="top_mid",    y=16,  font=14, color=C_GREY})
local lbl_pct     = ui.label(scr, "",          {align="bottom_mid", y=-16, font=14, color=C_GREY})
local lbl_flash   = ui.label(scr, "",          {align="center",     y=-52, font=14, color=C_MAG})

-- State
local last_layer   = -1
local flash_until  = 0
local FLASH_MS     = 800  -- how long the "PHOTO" flash shows

local function draw_ring(pct, color)
    local sweep = math.floor(360 * pct / 100)
    ui.canvas_arc(canvas, CX, CY, 104, color, 6, -90, -90 + sweep)
    -- track
    ui.canvas_arc(canvas, CX, CY, 104, 0x2104, 6, -90 + sweep, 270)
end

local function update()
    local printing   = bambu.printing()
    local layer      = bambu.layer()
    local total      = bambu.total_layers()
    local pct        = bambu.progress()
    local state      = bambu.state()
    local now        = sys.millis()

    -- Status label
    if not printing then
        ui.label_set(lbl_status, state)
        ui.label_color(lbl_status, C_GREY)
    else
        ui.label_set(lbl_status, "PRINTING")
        ui.label_color(lbl_status, C_GRN)
    end

    -- Layer numbers
    if total > 0 then
        ui.label_set(lbl_layer, tostring(layer))
        ui.label_set(lbl_total, "of " .. tostring(total))
        ui.label_set(lbl_pct,   pct .. "%")
    end

    -- Detect layer change → flash "PHOTO"
    if printing and layer ~= last_layer and last_layer ~= -1 then
        flash_until = now + FLASH_MS
    end
    last_layer = layer

    -- Flash label
    if now < flash_until then
        ui.label_set(lbl_flash, "PHOTO")
        ui.label_color(lbl_flash, C_MAG)
    else
        ui.label_set(lbl_flash, "")
    end

    -- Progress ring
    ui.canvas_clear(canvas, C_BG)
    if printing and total > 0 then
        draw_ring(pct, C_GRN)
    else
        ui.canvas_arc(canvas, CX, CY, 104, 0x2104, 6, 0, 360)
    end
end

sys.on_tick(update)
