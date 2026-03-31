-- @name Crosshair
-- @color 0x07E0
-- @sdk_min 2
-- @version 1.0
-- @author BambuHelper
-- @category monitor
-- @description Animated targeting crosshair shown during toolhead homing. Exits when printer is idle.

local CLR_BG     = 0x0000
local CLR_CYAN   = 0x07FF
local CLR_GREEN  = 0x07E0
local CLR_RED    = 0xF800
local CLR_DIM    = 0x2104
local CLR_WHITE  = 0xFFFF

local W, H, CX, CY = 240, 240, 120, 120

local scr    = ui.screen()
local canvas = ui.canvas(scr, W, H, 0, 0)

-- ── Animation state ──────────────────────────────────────────────────────────
local sweep_angle = 0       -- rotating sweep arm
local ping_r      = 0       -- sonar ping expanding circle
local ping_opa    = 255     -- ping opacity (fades as it expands)
local blink_t     = 0       -- centre dot blink timer

-- Crosshair gap (lines stop short of centre, creating a gap)
local GAP  = 22
local ROUT = 90   -- outer circle radius
local RMID = 55   -- sweep arm radius
local RPIN = 8    -- centre dot radius

-- Tick marks around outer ring (every 30°)
local function draw_ticks()
    for a = 0, 330, 30 do
        local rad  = math.rad(a)
        local x1   = CX + math.floor((ROUT - 4) * math.cos(rad))
        local y1   = CY + math.floor((ROUT - 4) * math.sin(rad))
        local x2   = CX + math.floor((ROUT + 4) * math.cos(rad))
        local y2   = CY + math.floor((ROUT + 4) * math.sin(rad))
        ui.canvas_line(canvas, x1, y1, x2, y2, CLR_DIM, 1)
    end
end

-- Main redraw every tick
local function redraw(dt)
    ui.canvas_clear(canvas, CLR_BG)

    -- Outer ring
    ui.canvas_circle(canvas, CX, CY, ROUT, CLR_CYAN, 1)

    -- Dim inner guide circle
    ui.canvas_circle(canvas, CX, CY, RMID, CLR_DIM, 1)

    -- Tick marks
    draw_ticks()

    -- Crosshair lines (gap around centre)
    ui.canvas_line(canvas, CX, CY - ROUT + 2, CX, CY - GAP, CLR_GREEN, 1)
    ui.canvas_line(canvas, CX, CY + GAP,      CX, CY + ROUT - 2, CLR_GREEN, 1)
    ui.canvas_line(canvas, CX - ROUT + 2, CY, CX - GAP, CY, CLR_GREEN, 1)
    ui.canvas_line(canvas, CX + GAP,      CY, CX + ROUT - 2, CY, CLR_GREEN, 1)

    -- Diagonal accent lines (45°, short)
    local D = 14
    local O = math.floor(ROUT * 0.65)
    ui.canvas_line(canvas, CX - O, CY - O, CX - O + D, CY - O + D, CLR_DIM, 1)
    ui.canvas_line(canvas, CX + O, CY - O, CX + O - D, CY - O + D, CLR_DIM, 1)
    ui.canvas_line(canvas, CX - O, CY + O, CX - O + D, CY + O - D, CLR_DIM, 1)
    ui.canvas_line(canvas, CX + O, CY + O, CX + O - D, CY + O - D, CLR_DIM, 1)

    -- Rotating sweep arc (60° wide)
    ui.canvas_arc(canvas, CX, CY, RMID - 4, sweep_angle, sweep_angle + 60, CLR_GREEN, 3)

    -- Sonar ping: expanding circle that fades
    if ping_r > 0 then
        local col = CLR_CYAN
        ui.canvas_circle(canvas, CX, CY, ping_r, col, 1)
    end

    -- Centre dot (blinks)
    local show_dot = (blink_t // 500) % 2 == 0
    if show_dot then
        ui.canvas_circle(canvas, CX, CY, RPIN, CLR_RED, 2)
        -- small cross in dot
        ui.canvas_line(canvas, CX - 4, CY, CX + 4, CY, CLR_WHITE, 1)
        ui.canvas_line(canvas, CX, CY - 4, CX, CY + 4, CLR_WHITE, 1)
    else
        ui.canvas_circle(canvas, CX, CY, RPIN, CLR_DIM, 1)
    end
end

-- ── Tick ─────────────────────────────────────────────────────────────────────
sys.on_tick(function(dt)
    -- Advance sweep
    sweep_angle = (sweep_angle + 4) % 360

    -- Advance sonar ping
    ping_r = ping_r + math.floor(dt * 0.08)
    if ping_r > ROUT then ping_r = 0 end

    -- Blink timer
    blink_t = blink_t + dt

    redraw(dt)

    -- Long press exits via the C layer; no auto-exit here
end)
