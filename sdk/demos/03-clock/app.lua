-- @name Digital Clock
-- @color 0x07FF
-- @sdk_min 1
-- @version 1.0
-- @author BambuHelper
-- @category demo
-- @description Live digital clock with date using sys.time() and sys.every().

local CLR_TIME  = 0x07FF   -- cyan
local CLR_DATE  = 0xC618   -- dim white
local CLR_DOT   = 0xFFFF   -- white (blinking colon)
local CLR_NOSYNC = 0xFBE0  -- orange — shown before NTP syncs

local DAYS   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}
local MONTHS = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}

-- ── Screen ───────────────────────────────────────────────────────────────────
local scr = ui.screen()

-- Large time display — two labels so the colon can blink independently
local hour_lbl   = ui.label(scr, "--",  {align="center", x=-28, y=-14, font=40, color=CLR_TIME})
local colon_lbl  = ui.label(scr, ":",   {align="center", x=0,   y=-18, font=40, color=CLR_DOT})
local min_lbl    = ui.label(scr, "--",  {align="center", x=28,  y=-14, font=40, color=CLR_TIME})

-- Date line below the time
local date_lbl   = ui.label(scr, "Syncing…", {align="center", y=36, font=16, color=CLR_DATE})

-- Seconds bar at the bottom — a thin arc that sweeps 0→59
local sec_arc = ui.arc(scr, {cx=120, cy=120, size=200, value=0,
    color=CLR_TIME, track=0x18E3, thickness=4, start=135, sweep=270})

local colon_on = true

local function update()
    local t = sys.time()

    if not t.synced then
        ui.label_set(hour_lbl, "--")
        ui.label_set(min_lbl,  "--")
        ui.label_color(hour_lbl, CLR_NOSYNC)
        ui.label_color(min_lbl,  CLR_NOSYNC)
        ui.label_set(date_lbl, "No NTP sync")
        return
    end

    ui.label_color(hour_lbl, CLR_TIME)
    ui.label_color(min_lbl,  CLR_TIME)
    ui.label_set(hour_lbl, string.format("%02d", t.hour))
    ui.label_set(min_lbl,  string.format("%02d", t.min))

    -- Blink the colon every second
    colon_on = not colon_on
    ui.label_color(colon_lbl, colon_on and CLR_DOT or 0x0000)

    -- Date: "Mon 3 Jun"
    local day_name = DAYS[t.wday + 1]   or "?"
    local mon_name = MONTHS[t.month]    or "?"
    ui.label_set(date_lbl, day_name .. " " .. t.day .. " " .. mon_name)

    -- Seconds arc sweeps one full revolution per minute
    ui.arc_set(sec_arc, math.floor(t.sec / 59 * 100))
end

-- Run immediately so the screen isn't blank for the first second
update()
sys.every(1000, update)

sys.log("03-clock loaded")
