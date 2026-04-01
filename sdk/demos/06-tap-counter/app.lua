-- @name Tap Counter
-- @color 0xFFE0
-- @sdk_min 1
-- @version 1.0
-- @author BambuHelper
-- @category demo
-- @description Tap a button to count. Shows ui.on_tap, anim_size feedback, and persistent storage.

-- Demonstrates: ui.on_tap, ui.anim_size, ui.anim_fade,
--               sys.store_set, sys.store_get, sys.store_del

local CLR_BG      = 0x0000
local CLR_BTN     = 0x2945   -- dark blue-grey button face
local CLR_BTN_RIM = 0x4228
local CLR_COUNT   = 0xFFE0   -- yellow count
local CLR_LABEL   = 0xFFFF
local CLR_DIM     = 0xC618
local CLR_RESET   = 0xF800   -- red reset button

local STORE_KEY = "tap_demo_count"

-- ── Load persisted count ──────────────────────────────────────────────────────
local count = tonumber(sys.store_get(STORE_KEY, "0")) or 0

-- ── Screen ───────────────────────────────────────────────────────────────────
local scr = ui.screen()

-- Large count display
local count_lbl = ui.label(scr, tostring(count),
    {align="center", y=-30, font=40, color=CLR_COUNT})

-- "taps" sub-label
ui.label(scr, "taps", {align="center", y=18, font=16, color=CLR_DIM})

-- Persistent-save indicator (flashes when saved)
local saved_lbl = ui.label(scr, "saved ✓", {align="center", y=42, font=14, color=0x07E0})
ui.anim_fade(saved_lbl, 255, 0, {time=1})  -- hide immediately

-- Big tap target — centred circle
local btn = ui.rect(scr, {x=60, y=60, w=120, h=120, color=CLR_BTN, radius=60})

-- Reset button — small, bottom-centre
local reset_btn = ui.rect(scr, {x=90, y=192, w=60, h=28, color=CLR_RESET, radius=6})
ui.label(scr, "reset", {align="bottom_mid", y=-16, font=14, color=CLR_LABEL})

-- ── Tap handler ───────────────────────────────────────────────────────────────
local function pulse_btn()
    -- Scale up slightly then spring back — gives physical button feel
    ui.anim_stop(btn)
    ui.anim_size(btn, 132, 132, {time=80,  easing="linear"})
    sys.every(90, function()
        ui.anim_stop(btn)
        ui.anim_size(btn, 120, 120, {time=200, easing="bounce"})
    end)
end

ui.on_tap(btn, function()
    count = count + 1
    ui.label_set(count_lbl, tostring(count))
    pulse_btn()

    -- Persist every tap
    sys.store_set(STORE_KEY, tostring(count))

    -- Flash "saved ✓" label
    ui.anim_stop(saved_lbl)
    ui.anim_fade(saved_lbl, 0, 255, {time=150})
    sys.every(700, function()
        ui.anim_fade(saved_lbl, 255, 0, {time=400, easing="ease_out"})
    end)
end)

-- ── Reset handler ─────────────────────────────────────────────────────────────
ui.on_tap(reset_btn, function()
    count = 0
    ui.label_set(count_lbl, "0")
    sys.store_del(STORE_KEY)

    -- Brief color flash on the count to confirm reset
    ui.label_color(count_lbl, CLR_RESET)
    sys.every(400, function()
        ui.label_color(count_lbl, CLR_COUNT)
    end)
end)

sys.log("06-tap-counter loaded, count=" .. count)
