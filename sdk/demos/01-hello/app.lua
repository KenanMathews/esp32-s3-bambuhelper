-- @name Hello World
-- @color 0x07E0
-- @sdk_min 1
-- @version 1.0
-- @author BambuHelper
-- @category demo
-- @description The simplest possible app: a label, a subtitle, and an elapsed-time counter.

-- ── Screen ───────────────────────────────────────────────────────────────────
local scr = ui.screen()

-- Title label, centred, large green text
ui.label(scr, "Hello!", {align="center", y=-24, font=40, color=0x07E0})

-- Subtitle in dimmer, smaller text
ui.label(scr, "BambuHelper SDK", {align="center", y=18, font=16, color=0xC618})

-- Elapsed-time counter near the bottom
local elapsed_lbl = ui.label(scr, "0s", {align="center", y=60, font=14, color=0x07FF})

-- ── Timer ────────────────────────────────────────────────────────────────────
-- sys.every(ms, fn) fires fn() on an independent C timer.
-- No sys.on_tick registration needed.
local start_ms = sys.millis()

sys.every(1000, function()
    local s = math.floor((sys.millis() - start_ms) / 1000)
    ui.label_set(elapsed_lbl, s .. "s elapsed")
end)

sys.log("01-hello loaded")
