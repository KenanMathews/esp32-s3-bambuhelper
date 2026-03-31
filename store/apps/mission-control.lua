-- @name Mission Control
-- @color 0x0451
-- @sdk_min 2
-- @version 1.0
-- @author BambuHelper
-- @category monitor
-- @description Live print dashboard: progress ring, temps, layers, ETA. Auto-shows report on finish.

local CLR_GREEN  = 0x07E0
local CLR_ORANGE = 0xFBE0
local CLR_BLUE   = 0x34DF
local CLR_CYAN   = 0x07FF
local CLR_RED    = 0xF800
local CLR_YELLOW = 0xFFE0
local CLR_TEXT   = 0xFFFF
local CLR_DIM    = 0xC618
local CLR_DARK   = 0x18E3

-- Track print start for elapsed time
local print_start_ms = sys.millis()
local was_printing   = bambu.printing()
local report_shown   = false

-- ── Build screen ───────────────────────────────────────────────────────────
local scr = ui.screen()

-- Outer progress ring (full circle, 270° sweep)
local prog_arc = ui.arc(scr, {cx=120, cy=120, size=108, value=0,
    color=CLR_GREEN, track=CLR_DARK, thickness=10})

-- Middle nozzle ring
local noz_arc = ui.arc(scr, {cx=120, cy=120, size=90, value=0,
    color=CLR_ORANGE, track=CLR_DARK, thickness=7})

-- Inner bed ring
local bed_arc = ui.arc(scr, {cx=120, cy=120, size=75, value=0,
    color=CLR_BLUE, track=CLR_DARK, thickness=7})

-- Center: big percent
local pct_lbl = ui.label(scr, "0%", {align="center", y=-18, font=28, color=CLR_TEXT})

-- Job name below percent
local job_lbl = ui.label(scr, bambu.job_name(), {align="center", y=14, font=14, color=CLR_DIM, w=160})

-- ETA line
local eta_lbl = ui.label(scr, "--:--", {align="center", y=32, font=14, color=CLR_CYAN})

-- Layer counter top-left area
local layer_lbl = ui.label(scr, "L0/0", {x=18, y=100, font=14, color=CLR_DIM})

-- Speed indicator top-right area
local speed_lbl = ui.label(scr, "", {x=160, y=100, font=14, color=CLR_YELLOW})

-- Nozzle temp bottom-left
local noz_lbl = ui.label(scr, "N --°", {x=20, y=190, font=14, color=CLR_ORANGE})

-- Bed temp bottom-right
local bed_lbl = ui.label(scr, "B --°", {x=145, y=190, font=14, color=CLR_BLUE})

-- State badge top-center
local state_lbl = ui.label(scr, "IDLE", {align="top_mid", y=10, font=14, color=CLR_DIM})

-- ── Helpers ────────────────────────────────────────────────────────────────
local speed_names = {"Silent", "Std", "Sport", "Ludicrous"}

local function temp_pct(cur, target)
    if target <= 0 then return 0 end
    local p = math.floor(cur / target * 100)
    return math.min(p, 100)
end

local function fmt_eta(mins)
    if mins <= 0 then return "Done" end
    if mins < 60 then return mins .. "m" end
    return math.floor(mins/60) .. "h " .. (mins % 60) .. "m"
end

local function fmt_elapsed()
    local s = math.floor((sys.millis() - print_start_ms) / 1000)
    local h = math.floor(s / 3600)
    local m = math.floor((s % 3600) / 60)
    if h > 0 then return h .. "h " .. m .. "m" end
    return m .. "m " .. (s % 60) .. "s"
end

local function state_color(st)
    if st == "RUNNING"  then return CLR_GREEN  end
    if st == "PAUSE"    then return CLR_YELLOW end
    if st == "FAILED"   then return CLR_RED    end
    if st == "FINISH"   then return CLR_CYAN   end
    return CLR_DIM
end

-- ── Save report and exit on FINISH ─────────────────────────────────────────
local function save_and_exit()
    local elapsed = fmt_elapsed()
    sys.store_set("last_report_job",     bambu.job_name())
    sys.store_set("last_report_elapsed", elapsed)
    sys.store_set("last_report_layers",  tostring(bambu.total_layers()))
    sys.beep()
    sys.exit()
end

-- ── Tick ───────────────────────────────────────────────────────────────────
sys.on_tick(function(_dt)
    local state    = bambu.state()
    local printing = bambu.printing()

    -- Detect print start (for elapsed timer reset)
    if printing and not was_printing then
        print_start_ms = sys.millis()
    end
    was_printing = printing

    -- Save report and exit on FINISH
    if state == "FINISH" and not report_shown then
        report_shown = true
        save_and_exit()
        return
    end

    -- If report is showing, nothing to update on the main screen
    if report_shown then return end

    -- ── Update main screen ───────────────────────────────────────────────
    local prog     = bambu.progress()
    local noz      = bambu.nozzle_temp()
    local noz_t    = bambu.nozzle_target()
    local bed      = bambu.bed_temp()
    local bed_t    = bambu.bed_target()
    local layer    = bambu.layer()
    local total    = bambu.total_layers()
    local eta      = bambu.remaining_mins()
    local sp       = bambu.speed()

    -- Progress arc + label
    ui.arc_set(prog_arc, prog)
    local prog_color = prog >= 90 and CLR_CYAN or (prog >= 50 and CLR_GREEN or CLR_ORANGE)
    ui.arc_color(prog_arc, prog_color)
    ui.label_set(pct_lbl, prog .. "%")

    -- Temp arcs
    ui.arc_set(noz_arc, temp_pct(noz, noz_t))
    ui.arc_set(bed_arc, temp_pct(bed, bed_t))

    -- Nozzle color: orange→green when at target
    local noz_color = (noz_t > 0 and math.abs(noz - noz_t) < 5) and CLR_GREEN or CLR_ORANGE
    ui.arc_color(noz_arc, noz_color)

    -- Labels
    ui.label_set(job_lbl, bambu.job_name())
    ui.label_set(eta_lbl, printing and fmt_eta(eta) or fmt_elapsed())
    ui.label_set(layer_lbl, "L" .. layer .. "/" .. total)
    ui.label_set(speed_lbl, speed_names[sp] or "")
    ui.label_set(noz_lbl,  string.format("N %.0f°", noz))
    ui.label_set(bed_lbl,  string.format("B %.0f°", bed))
    ui.label_set(state_lbl, state)
    ui.label_color(state_lbl, state_color(state))
end)
