-- @name Printer Status
-- @color 0x34DF
-- @sdk_min 1
-- @version 1.0
-- @author BambuHelper
-- @category demo
-- @description Comprehensive bambu.* API showcase: state, progress, temps, fans, layers, AMS.

-- This demo is intentionally dense — it exercises every bambu.* call so you can
-- see what each one returns while a print is running (or while the printer is idle).

local CLR_GREEN  = 0x07E0
local CLR_ORANGE = 0xFBE0
local CLR_BLUE   = 0x34DF
local CLR_CYAN   = 0x07FF
local CLR_RED    = 0xF800
local CLR_YELLOW = 0xFFE0
local CLR_WHITE  = 0xFFFF
local CLR_DIM    = 0xC618
local CLR_DARK   = 0x18E3

local SPEED_NAMES = {"Silent", "Std", "Sport", "Ludicrous"}

-- ── State colour helper ───────────────────────────────────────────────────────
local function state_color(s)
    if s == "PRINTING" then return CLR_GREEN  end
    if s == "PAUSE"    then return CLR_YELLOW end
    if s == "FAILED"   then return CLR_RED    end
    if s == "FINISH"   then return CLR_CYAN   end
    return CLR_DIM
end

-- ── Screen ───────────────────────────────────────────────────────────────────
local scr = ui.screen()

-- Progress arc (large, outer)
local prog_arc = ui.arc(scr, {cx=120, cy=105, size=120, value=0,
    color=CLR_GREEN, track=CLR_DARK, thickness=10})

-- Progress % — centre of arc
local prog_lbl = ui.label(scr, "0%", {align="center", y=-20, font=28, color=CLR_WHITE})

-- Printer name — top
local name_lbl = ui.label(scr, bambu.printer_name(),
    {align="top_mid", y=10, font=14, color=CLR_DIM, w=180})

-- State badge below percent
local state_lbl = ui.label(scr, "IDLE", {align="center", y=12, font=14, color=CLR_DIM})

-- Layer counter — left of centre
local layer_lbl = ui.label(scr, "L 0/0", {x=14, y=92, font=14, color=CLR_DIM})

-- Speed — right of centre
local speed_lbl = ui.label(scr, "", {x=148, y=92, font=14, color=CLR_YELLOW})

-- ETA or elapsed — below state
local eta_lbl = ui.label(scr, "--", {align="center", y=32, font=14, color=CLR_CYAN})

-- Nozzle temp row
local noz_arc = ui.arc(scr, {cx=66, cy=178, size=52, value=0,
    color=CLR_ORANGE, track=CLR_DARK, thickness=6})
local noz_lbl = ui.label(scr, "N --°", {x=40,  y=165, font=14, color=CLR_ORANGE})

-- Bed temp row
local bed_arc = ui.arc(scr, {cx=174, cy=178, size=52, value=0,
    color=CLR_BLUE,   track=CLR_DARK, thickness=6})
local bed_lbl = ui.label(scr, "B --°", {x=148, y=165, font=14, color=CLR_BLUE})

-- Chamber temp — bottom centre
local chamber_lbl = ui.label(scr, "C --°",
    {align="bottom_mid", y=-10, font=14, color=CLR_DIM})

-- AMS tray indicators — four dots near bottom-left
local AMS_X = {16, 30, 44, 58}
local AMS_Y = 212
local ams_dots = {}
for i = 1, 4 do
    ams_dots[i] = ui.rect(scr, {x=AMS_X[i], y=AMS_Y, w=10, h=10,
        color=CLR_DARK, radius=5})
end
local ams_lbl = ui.label(scr, "AMS", {x=14, y=198, font=14, color=CLR_DIM})

-- ── Helpers ───────────────────────────────────────────────────────────────────
local function temp_pct(cur, tgt)
    if tgt <= 0 then return 0 end
    return math.min(math.floor(cur / tgt * 100), 100)
end

local function fmt_eta(mins)
    if mins <= 0 then return "Done" end
    if mins < 60  then return mins .. "m" end
    return math.floor(mins / 60) .. "h " .. (mins % 60) .. "m"
end

-- ── Tick ─────────────────────────────────────────────────────────────────────
sys.on_tick(function(_dt)
    local state    = bambu.state()
    local prog     = bambu.progress()
    local printing = bambu.printing()
    local layer    = bambu.layer()
    local total    = bambu.total_layers()
    local eta      = bambu.remaining_mins()
    local sp       = bambu.speed()
    local noz      = bambu.nozzle_temp()
    local noz_t    = bambu.nozzle_target()
    local bed      = bambu.bed_temp()
    local bed_t    = bambu.bed_target()
    local chamber  = bambu.chamber_temp()
    local active   = bambu.ams_active()

    -- Progress
    ui.arc_set(prog_arc, prog)
    local prog_color = prog >= 90 and CLR_CYAN or (prog >= 50 and CLR_GREEN or CLR_ORANGE)
    ui.arc_color(prog_arc, prog_color)
    ui.label_set(prog_lbl, prog .. "%")
    ui.label_set(prog_lbl, prog .. "%")

    -- State
    ui.label_set(state_lbl, state)
    ui.label_color(state_lbl, state_color(state))

    -- Layer / speed
    ui.label_set(layer_lbl, "L " .. layer .. "/" .. total)
    ui.label_set(speed_lbl, SPEED_NAMES[sp + 1] or "")

    -- ETA
    ui.label_set(eta_lbl, printing and fmt_eta(eta) or "")

    -- Nozzle
    local noz_at = noz_t > 0 and math.abs(noz - noz_t) < 5
    ui.arc_set(noz_arc, temp_pct(noz, noz_t))
    ui.arc_color(noz_arc, noz_at and CLR_GREEN or CLR_ORANGE)
    ui.label_set(noz_lbl, string.format("N %.0f°", noz))

    -- Bed
    ui.arc_set(bed_arc, temp_pct(bed, bed_t))
    ui.label_set(bed_lbl, string.format("B %.0f°", bed))

    -- Chamber
    ui.label_set(chamber_lbl, string.format("C %.0f°", chamber))

    -- AMS dots — colour each tray from bambu.ams_color()
    for i = 1, 4 do
        local tray_idx = i - 1
        local col = bambu.ams_color(tray_idx)
        local is_active = (tray_idx == active)
        if col > 0 then
            ui.rect_set(ams_dots[i], col)
            -- Pulse the active tray
            if is_active then
                ui.anim_size(ams_dots[i], 14, 14, {time=400, easing="overshoot"})
            else
                ui.rect_size(ams_dots[i], 10, 10)
            end
        else
            ui.rect_set(ams_dots[i], CLR_DARK)
            ui.rect_size(ams_dots[i], 10, 10)
        end
    end
end)

sys.log("07-printer-status loaded")
