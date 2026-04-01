-- @name Canvas Clock
-- @color 0x4A10
-- @sdk_min 1
-- @version 1.0
-- @author BambuHelper
-- @category demo
-- @description Analog clock face drawn entirely with canvas primitives. Updates every second.

-- Demonstrates: ui.canvas, canvas_clear, canvas_circle, canvas_line, canvas_arc
-- Every second the canvas is cleared and all hands are redrawn from scratch.

local W, H, CX, CY = 240, 240, 120, 120
local R_FACE   = 108   -- outer ring radius
local R_HOUR   =  52   -- hour hand length
local R_MIN    =  72   -- minute hand length
local R_SEC    =  82   -- second hand length
local R_DOT    =   5   -- centre pivot dot

local CLR_BG      = 0x0000
local CLR_FACE    = 0x1082   -- very dark grey ring
local CLR_TICK    = 0x630C   -- dim tick marks
local CLR_TICK_5  = 0xC618   -- brighter tick every 5 minutes
local CLR_HOUR    = 0xFFFF   -- white hour hand
local CLR_MIN     = 0xFFFF   -- white minute hand
local CLR_SEC     = 0xF800   -- red second hand
local CLR_PIVOT   = 0xF800   -- red centre dot
local CLR_NUMBERS = 0x8410   -- dim digit positions (we draw ticks, not numbers)

-- ── Screen & canvas ──────────────────────────────────────────────────────────
local scr    = ui.screen()
local canvas = ui.canvas(scr, W, H, 0, 0)

if not canvas then
    -- PSRAM allocation failed — show an error label instead
    ui.label(scr, "Canvas\nnot available", {align="center", font=20, color=0xF800})
    sys.log("05-canvas-clock: canvas alloc failed")
    return
end

-- ── Drawing helpers ───────────────────────────────────────────────────────────
local function hand_endpoint(angle_deg, length)
    -- 0° = 12 o'clock, clockwise
    local rad = math.rad(angle_deg - 90)
    return CX + math.floor(length * math.cos(rad)),
           CY + math.floor(length * math.sin(rad))
end

local function draw_face()
    -- Outer ring
    ui.canvas_circle(canvas, CX, CY, R_FACE, CLR_FACE, 2)

    -- 60 tick marks; longer/brighter every 5
    for i = 0, 59 do
        local a   = math.rad(i * 6 - 90)
        local is5 = (i % 5 == 0)
        local r1  = R_FACE - (is5 and 10 or 5)
        local r2  = R_FACE - 2
        local x1  = CX + math.floor(r1 * math.cos(a))
        local y1  = CY + math.floor(r1 * math.sin(a))
        local x2  = CX + math.floor(r2 * math.cos(a))
        local y2  = CY + math.floor(r2 * math.sin(a))
        local clr = is5 and CLR_TICK_5 or CLR_TICK
        local w   = is5 and 2 or 1
        ui.canvas_line(canvas, x1, y1, x2, y2, clr, w)
    end
end

local function draw_hand(length, angle_deg, color, width)
    local x, y = hand_endpoint(angle_deg, length)
    ui.canvas_line(canvas, CX, CY, x, y, color, width)
end

local last_hour, last_min = -1, -1   -- track when we need a full redraw

local function redraw(t)
    local sec_angle  = t.sec  * 6           -- 0–354°
    local min_angle  = t.min  * 6  + t.sec  * 0.1
    local hour_angle = (t.hour % 12) * 30 + t.min * 0.5

    ui.canvas_clear(canvas, CLR_BG)
    draw_face()

    -- Hour hand (thickest)
    draw_hand(R_HOUR, hour_angle, CLR_HOUR, 5)
    -- Minute hand
    draw_hand(R_MIN,  min_angle,  CLR_MIN,  3)
    -- Second hand (thin red)
    draw_hand(R_SEC,  sec_angle,  CLR_SEC,  1)
    -- Small back-tail for the second hand (adds realism)
    local tx, ty = hand_endpoint(sec_angle + 180, 18)
    ui.canvas_line(canvas, CX, CY, tx, ty, CLR_SEC, 1)

    -- Centre pivot dot over all hands
    ui.canvas_circle(canvas, CX, CY, R_DOT, CLR_PIVOT, R_DOT)
end

-- ── Tick ─────────────────────────────────────────────────────────────────────
local function update()
    local t = sys.time()
    if not t.synced then
        -- Draw a "waiting" state — rotating second hand at midnight position
        local fake = {hour=0, min=0, sec=math.floor(sys.millis()/1000) % 60, synced=false}
        redraw(fake)
        return
    end
    redraw(t)
end

update()
sys.every(1000, update)

sys.log("05-canvas-clock loaded")
