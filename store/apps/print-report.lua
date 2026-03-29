-- @name Print Report
-- @color 0x0228
-- @sdk_min 2
-- @version 1.0
-- @author BambuHelper
-- @category monitor
-- @description Shows the last completed print report: job name, duration, layers, temps.

local CLR_CYAN   = 0x07FF
local CLR_TEXT   = 0xFFFF
local CLR_DIM    = 0xC618
local CLR_ORANGE = 0xFBE0
local CLR_BLUE   = 0x34DF
local CLR_GREEN  = 0x07E0
local CLR_RED    = 0xF800

-- Load last saved report from persistent store
local job     = sys.store_get("last_report_job",     "No recent print")
local elapsed = sys.store_get("last_report_elapsed", "—")
local layers  = sys.store_get("last_report_layers",  "—")

-- ── Build screen ───────────────────────────────────────────────────────────
local scr = ui.screen()

ui.label(scr, "Print Report", {align="top_mid", y=12, font=16, color=CLR_CYAN})

-- Job name (may be long — clip it)
ui.label(scr, job, {align="top_mid", y=46, font=14, color=CLR_TEXT, w=200})

-- Stats table
local rows = {
    {"Duration", elapsed, CLR_GREEN},
    {"Layers",   layers,  CLR_TEXT},
    {"Nozzle",   string.format("%.0f / %.0f°C", bambu.nozzle_temp(), bambu.nozzle_target()), CLR_ORANGE},
    {"Bed",      string.format("%.0f / %.0f°C", bambu.bed_temp(),    bambu.bed_target()),    CLR_BLUE},
    {"State",    bambu.state(), CLR_DIM},
}

local y = 72
for _, row in ipairs(rows) do
    ui.label(scr, row[1], {x=24,  y=y, font=14, color=CLR_DIM})
    ui.label(scr, row[2], {x=105, y=y, font=14, color=row[3]})
    y = y + 22
end

ui.label(scr, "Long press to exit", {align="bottom_mid", y=-8, font=14, color=CLR_DIM})

ui.show(scr)

-- Static display — no tick loop needed, long-press exits via launcher
