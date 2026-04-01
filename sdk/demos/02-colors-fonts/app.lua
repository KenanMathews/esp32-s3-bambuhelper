-- @name Colors & Fonts
-- @color 0xF81F
-- @sdk_min 1
-- @version 1.0
-- @author BambuHelper
-- @category demo
-- @description Static showcase of all font sizes, ui.color(), and label alignment.

-- ── Screen ───────────────────────────────────────────────────────────────────
-- No tick loop or timer needed — this app is fully static.
local scr = ui.screen()

-- ui.color(r, g, b) converts 8-bit RGB values to the RGB565 format used everywhere.
-- All the color constants you see in other apps (0x07E0, 0xF800…) are just
-- pre-calculated RGB565 values — ui.color() is the readable alternative.

-- Largest font — centred, offset upward
ui.label(scr, "Font 40",  {align="center", y=-74, font=40, color=ui.color(255, 100,   0)})

-- Each smaller step fits below the previous
ui.label(scr, "Font 28",  {align="center", y=-28, font=28, color=ui.color(  0, 200, 255)})
ui.label(scr, "Font 20",  {align="center", y=10,  font=20, color=ui.color(100, 255, 100)})
ui.label(scr, "Font 16",  {align="center", y=40,  font=16, color=0xFFFF})
ui.label(scr, "Font 14",  {align="center", y=62,  font=14, color=0xC618})

-- Alignment demo — pin labels to the corners of the round face
ui.label(scr, "TL", {align="top_left",     x=28, y=28, font=14, color=ui.color(255, 80, 80)})
ui.label(scr, "TR", {align="top_right",    x=-28,y=28, font=14, color=ui.color(255, 80, 80)})
ui.label(scr, "BL", {align="bottom_left",  x=28, y=-28,font=14, color=ui.color(255, 80, 80)})
ui.label(scr, "BR", {align="bottom_right", x=-28,y=-28,font=14, color=ui.color(255, 80, 80)})

sys.log("02-colors-fonts loaded")
