-- @name Animations
-- @color 0xFBE0
-- @sdk_min 1
-- @version 1.0
-- @author BambuHelper
-- @category demo
-- @description Interactive tour of all animation types and every easing curve. Tap to advance.

-- Each stage demonstrates one animation function.
-- Tapping anywhere cycles through them in order.

local CLR_BG     = 0x0000
local CLR_GREEN  = 0x07E0
local CLR_CYAN   = 0x07FF
local CLR_ORANGE = 0xFBE0
local CLR_RED    = 0xF800
local CLR_BLUE   = 0x34DF
local CLR_YELLOW = 0xFFE0
local CLR_WHITE  = 0xFFFF
local CLR_DIM    = 0xC618
local CLR_DARK   = 0x18E3

-- Easing names cycle through all 7 curves on the arc stage
local EASINGS = {"linear", "ease_in", "ease_out", "ease_in_out",
                 "overshoot", "bounce", "step"}

-- ── Screen ───────────────────────────────────────────────────────────────────
local scr = ui.screen()

-- Stage title — top of screen
local stage_lbl  = ui.label(scr, "1 / 5  anim_fade",  {align="top_mid",    y=14,  font=14, color=CLR_CYAN})
local hint_lbl   = ui.label(scr, "tap anywhere",       {align="bottom_mid", y=-14, font=14, color=CLR_DIM})

-- ── Stage widgets ────────────────────────────────────────────────────────────

-- Stage 1: anim_fade — label fades in/out
local fade_lbl = ui.label(scr, "Fade", {align="center", y=0, font=40, color=CLR_GREEN})

-- Stage 2: anim_move — label slides between two positions
local move_lbl = ui.label(scr, "Move →", {align="center", y=0, font=28, color=CLR_ORANGE})

-- Stage 3: anim_arc — arc sweeps in with a selected easing
local arc      = ui.arc(scr,  {cx=120, cy=120, size=130, value=0,
    color=CLR_BLUE, track=CLR_DARK, thickness=14})
local arc_pct  = ui.label(scr, "0%",        {align="center", y=0,   font=28, color=CLR_WHITE})
local ease_lbl = ui.label(scr, EASINGS[1],  {align="center", y=34,  font=14, color=CLR_CYAN})

-- Stage 4: anim_size — rect pops from tiny to full size
local box      = ui.rect(scr, {x=95, y=95, w=50, h=50, color=CLR_RED, radius=8})

-- Stage 5: chained — fade-in then move (shows delay opt)
local chain_lbl = ui.label(scr, "chain", {align="center", y=30, font=28, color=CLR_YELLOW})

-- Hide all stage widgets initially (opacity 0)
local function hide(w) ui.anim_fade(w, 255, 0, {time=1}) end
hide(move_lbl); hide(arc); hide(arc_pct); hide(ease_lbl)
hide(box); hide(chain_lbl)

-- ── Stage logic ──────────────────────────────────────────────────────────────
local stage       = 1
local ease_idx    = 1
local arc_to_high = true  -- direction toggle for arc stage
local move_right  = true  -- direction toggle for move stage

local function run_stage()
    if stage == 1 then
        -- anim_fade: fade the label in and out, repeat
        ui.anim_fade(fade_lbl, 0, 255, {time=800, ["repeat"]=true, bounce=true, easing="ease_in_out"})

    elseif stage == 2 then
        -- anim_move: slide left ↔ right, repeat with bounce
        if move_right then
            ui.anim_move(move_lbl, 150, 120, {time=600, easing="overshoot"})
        else
            ui.anim_move(move_lbl, 70, 120, {time=600, easing="overshoot"})
        end
        move_right = not move_right

    elseif stage == 3 then
        -- anim_arc: sweep 0→100 or 100→0 with cycling easings
        local e    = EASINGS[ease_idx]
        local from = arc_to_high and 0 or 100
        local to_v = arc_to_high and 100 or 0
        arc_to_high = not arc_to_high
        ui.anim_stop(arc)
        ui.anim_arc(arc, from, to_v, {time=900, easing=e})
        ui.label_set(ease_lbl, e)
        ui.label_set(arc_pct,  (arc_to_high and "0" or "100") .. "%")
        ease_idx = (ease_idx % #EASINGS) + 1

    elseif stage == 4 then
        -- anim_size: box pulses between small and large
        ui.anim_stop(box)
        ui.anim_size(box, 110, 110, {time=500, easing="overshoot"})
        sys.every(600, function()
            ui.anim_stop(box)
            ui.anim_size(box, 50, 50, {time=400, easing="bounce"})
        end)

    elseif stage == 5 then
        -- chain: fade in, then move to centre from bottom, using delay
        ui.anim_fade(chain_lbl, 0, 255, {time=400, delay=0,   easing="ease_in"})
        ui.anim_move(chain_lbl, 120, 110, {time=500, delay=350, easing="overshoot"})
    end
end

local STAGE_TITLES = {
    "1/5  anim_fade",
    "2/5  anim_move",
    "3/5  anim_arc  (tap=next easing)",
    "4/5  anim_size",
    "5/5  chained (fade + move)",
}

-- Widgets visible per stage
local function set_stage_visibility(s)
    local show_fade  = s == 1
    local show_move  = s == 2
    local show_arc   = s == 3
    local show_box   = s == 4
    local show_chain = s == 5
    local t = 200
    ui.anim_fade(fade_lbl,   show_fade  and 255 or 0, 0, {time=1})
    ui.anim_fade(fade_lbl,   0, show_fade  and 255 or 0, {time=t})
    ui.anim_fade(move_lbl,   0, show_move  and 255 or 0, {time=t})
    ui.anim_fade(arc,        0, show_arc   and 255 or 0, {time=t})
    ui.anim_fade(arc_pct,    0, show_arc   and 255 or 0, {time=t})
    ui.anim_fade(ease_lbl,   0, show_arc   and 255 or 0, {time=t})
    ui.anim_fade(box,        0, show_box   and 255 or 0, {time=t})
    ui.anim_fade(chain_lbl,  0, show_chain and 255 or 0, {time=t})
end

local function advance()
    -- Stop repeating anims from previous stage
    ui.anim_stop(fade_lbl)
    ui.anim_stop(move_lbl)
    ui.anim_stop(arc)
    ui.anim_stop(box)
    ui.anim_stop(chain_lbl)

    stage = (stage % 5) + 1
    ui.label_set(stage_lbl, STAGE_TITLES[stage])
    set_stage_visibility(stage)

    -- One-shot delay so the fade-in completes before the anim fires
    sys.after(220, run_stage)
end

-- Tap anywhere — transparent full-screen overlay
local overlay = ui.rect(scr, {x=0, y=0, w=240, h=240, color=0x0000})
ui.on_tap(overlay, advance)

-- Start stage 1
set_stage_visibility(1)
run_stage()

sys.log("04-animations loaded")
