-- @name BLE Presence Guard
-- @color 0x001F
-- @sdk_min 2
-- @version 1.0
-- @author BambuHelper
-- @category tool
-- @description Watches for your phone via BLE while printing. Alerts if you wander too far and leave the print unattended.

-- ─────────────────────────────────────────────────────────────────────────────
--  CONFIGURATION (tap the crown icon to edit via store settings, or set here)
-- ─────────────────────────────────────────────────────────────────────────────
-- Store your phone's BLE MAC in persistent storage once:
--   sys.store_set("presence_mac", "AA:BB:CC:DD:EE:FF")
-- Or set WATCH_MAC directly below for a fixed device.
local WATCH_MAC    = sys.store_get("presence_mac", "")  -- "" = scan all, alert on any loss
local SCAN_MS      = 2000       -- scan duration per sweep
local RSSI_NEAR    = -70        -- dBm — closer than this = "present"
local RSSI_FAR     = -85        -- dBm — farther than this = "gone" (hysteresis band)
local ALERT_GRACE  = 15000      -- ms before alerting after signal lost (15s)
local SCAN_INTERVAL= 5000       -- ms between scans

-- ─────────────────────────────────────────────────────────────────────────────
--  Colors
-- ─────────────────────────────────────────────────────────────────────────────
local CLR_BG      = 0x0000
local CLR_TEXT    = 0xFFFF
local CLR_DIM     = 0x8410
local CLR_GREEN   = 0x07E0
local CLR_RED     = 0xF800
local CLR_ORANGE  = 0xFBE0
local CLR_CYAN    = 0x07FF
local CLR_BLUE    = 0x001F
local CLR_YELLOW  = 0xFFE0

local CX, CY = 120, 120
local fi = math.floor

-- ─────────────────────────────────────────────────────────────────────────────
--  State machine
-- ─────────────────────────────────────────────────────────────────────────────
local STATE_IDLE      = "idle"       -- not printing, monitoring passively
local STATE_WATCHING  = "watching"   -- printing + phone present
local STATE_SEARCHING = "searching"  -- phone not seen, within grace period
local STATE_ALERT     = "alert"      -- phone gone too long during print
local STATE_SNOOZED   = "snoozed"    -- alert acknowledged, not repeating

local state        = STATE_IDLE
local last_rssi    = -100
local last_seen_ms = sys.millis()
local lost_since   = 0             -- when phone first disappeared
local snooze_until = 0
local scan_due     = 0             -- next scan time
local scanning     = false
local last_scan_count = 0

-- ─────────────────────────────────────────────────────────────────────────────
--  Screen
-- ─────────────────────────────────────────────────────────────────────────────
local scr    = ui.screen()
local canvas = ui.canvas(scr, 240, 240, 0, 0)

-- Labels
local lbl_status  = ui.label(scr, "BLE Guard",    {align="top_mid",    y=14,  font=16, color=CLR_CYAN})
local lbl_icon    = ui.label(scr, "[BLE]",         {align="center",     y=-28, font=28, color=CLR_TEXT})
local lbl_main    = ui.label(scr, "Waiting...",    {align="center",     y=8,   font=20, color=CLR_DIM})
local lbl_rssi    = ui.label(scr, "",              {align="center",     y=38,  font=14, color=CLR_DIM})
local lbl_print   = ui.label(scr, "",              {align="bottom_mid", y=-34, font=14, color=CLR_DIM})
local lbl_hint    = ui.label(scr, "tap to snooze", {align="bottom_mid", y=-14, font=14, color=CLR_DIM})

-- ─────────────────────────────────────────────────────────────────────────────
--  RSSI → distance estimate string
-- ─────────────────────────────────────────────────────────────────────────────
local function rssi_to_dist(rssi)
  if rssi >= -55 then return "Very close"
  elseif rssi >= -67 then return "~1-2 m"
  elseif rssi >= -75 then return "~3-5 m"
  elseif rssi >= -85 then return "~5-10 m"
  else return "Far / lost"
  end
end

-- ─────────────────────────────────────────────────────────────────────────────
--  Draw ring (signal strength visualisation)
-- ─────────────────────────────────────────────────────────────────────────────
local function signal_pct(rssi)
  -- Map -100 dBm → 0%,  -40 dBm → 100%
  local clamped = math.max(-100, math.min(-40, rssi))
  return fi((clamped + 100) / 60 * 100)
end

local function draw_ring(rssi, ring_clr)
  ui.canvas_clear(canvas, CLR_BG)
  -- Track
  ui.canvas_arc(canvas, CX, CY, 100, 0, 360, CLR_DIM, 12)
  -- Fill
  local pct = signal_pct(rssi)
  if pct > 0 then
    ui.canvas_arc(canvas, CX, CY, 100, 0, fi(pct / 100 * 360), ring_clr, 12)
  end
  -- Pulse ring when scanning
  if scanning then
    ui.canvas_arc(canvas, CX, CY, 114, 0, 360, 0x2945, 2)
  end
end

local function draw_alert_ring()
  ui.canvas_clear(canvas, CLR_BG)
  -- Flashing red arcs
  local now = sys.millis()
  local blink = (fi(now / 500) % 2 == 0)
  if blink then
    ui.canvas_arc(canvas, CX, CY, 108, 0, 360, CLR_RED, 14)
  else
    ui.canvas_arc(canvas, CX, CY, 108, 0, 360, 0x4000, 14)
  end
  -- Print progress inner arc
  local pct = bambu.progress()
  ui.canvas_arc(canvas, CX, CY, 88, 0, 360, CLR_DIM, 8)
  ui.canvas_arc(canvas, CX, CY, 88, 0, fi(pct / 100 * 360), CLR_ORANGE, 8)
end

-- ─────────────────────────────────────────────────────────────────────────────
--  Scan + parse
-- ─────────────────────────────────────────────────────────────────────────────
local function do_scan()
  scanning = true
  local devices = ble.scan(SCAN_MS)
  scanning = false
  last_scan_count = #devices

  local best_rssi = -200
  local found = false

  for _, dev in ipairs(devices) do
    local mac = dev.addr or ""
    -- If a specific MAC is configured, only match that one
    if WATCH_MAC == "" or string.upper(mac) == string.upper(WATCH_MAC) then
      if (dev.rssi or -200) > best_rssi then
        best_rssi = dev.rssi
        found = true
      end
    end
  end

  if found and best_rssi > RSSI_FAR then
    last_rssi    = best_rssi
    last_seen_ms = sys.millis()
    lost_since   = 0
    return true, best_rssi
  end

  -- Not found or too weak
  if lost_since == 0 then
    lost_since = sys.millis()
  end
  return false, last_rssi
end

-- ─────────────────────────────────────────────────────────────────────────────
--  Update UI per state
-- ─────────────────────────────────────────────────────────────────────────────
local function update_ui()
  local prt_state = bambu.state()
  local printing  = bambu.printing()
  local pct       = bambu.progress()
  local print_str = printing and (prt_state .. " " .. pct .. "%") or "Not printing"

  ui.label_set(lbl_print, print_str)

  if state == STATE_IDLE then
    draw_ring(-60, CLR_DIM)
    ui.label_set(lbl_status, "BLE Guard")
    ui.label_color(lbl_status, CLR_CYAN)
    ui.label_set(lbl_icon, "[--]")
    ui.label_set(lbl_main, "Monitoring...")
    ui.label_color(lbl_main, CLR_DIM)
    ui.label_set(lbl_rssi, last_scan_count .. " devices nearby")
    ui.label_set(lbl_hint, "Prints only")

  elseif state == STATE_WATCHING then
    draw_ring(last_rssi, CLR_GREEN)
    ui.label_set(lbl_status, "Present")
    ui.label_color(lbl_status, CLR_GREEN)
    ui.label_set(lbl_icon, "[OK]")
    ui.label_set(lbl_main, rssi_to_dist(last_rssi))
    ui.label_color(lbl_main, CLR_GREEN)
    ui.label_set(lbl_rssi, last_rssi .. " dBm")
    ui.label_set(lbl_hint, "tap to snooze")

  elseif state == STATE_SEARCHING then
    local elapsed = fi((sys.millis() - lost_since) / 1000)
    local grace   = fi(ALERT_GRACE / 1000)
    draw_ring(last_rssi, CLR_YELLOW)
    ui.label_set(lbl_status, "Searching...")
    ui.label_color(lbl_status, CLR_YELLOW)
    ui.label_set(lbl_icon, "[?]")
    ui.label_set(lbl_main, elapsed .. "s / " .. grace .. "s")
    ui.label_color(lbl_main, CLR_YELLOW)
    ui.label_set(lbl_rssi, "last: " .. last_rssi .. " dBm")
    ui.label_set(lbl_hint, "tap to snooze")

  elseif state == STATE_ALERT then
    draw_alert_ring()
    ui.label_set(lbl_status, "[!] ALERT [!]")
    ui.label_color(lbl_status, CLR_RED)
    ui.label_set(lbl_icon, "[X]")
    ui.label_set(lbl_main, "Phone lost!")
    ui.label_color(lbl_main, CLR_RED)
    local away = fi((sys.millis() - lost_since) / 1000)
    local m = fi(away / 60)
    local s = away % 60
    local away_str = m > 0 and (m .. "m " .. s .. "s") or (s .. "s")
    ui.label_set(lbl_rssi, "Away " .. away_str)
    ui.label_set(lbl_hint, "tap to snooze 5min")

  elseif state == STATE_SNOOZED then
    draw_ring(last_rssi, CLR_BLUE)
    ui.label_set(lbl_status, "Snoozed")
    ui.label_color(lbl_status, CLR_BLUE)
    ui.label_set(lbl_icon, "[zz]")
    local remaining = fi((snooze_until - sys.millis()) / 1000)
    ui.label_set(lbl_main, fi(remaining / 60) .. "m " .. (remaining % 60) .. "s")
    ui.label_color(lbl_main, CLR_DIM)
    ui.label_set(lbl_rssi, "")
    ui.label_set(lbl_hint, "tap to cancel snooze")
  end
end

-- ─────────────────────────────────────────────────────────────────────────────
--  Tap handler — snooze or cancel
-- ─────────────────────────────────────────────────────────────────────────────
ui.on_tap(canvas, function()
  if state == STATE_ALERT or state == STATE_SEARCHING then
    snooze_until = sys.millis() + 300000  -- 5 min
    state = STATE_SNOOZED
    sys.beep()
  elseif state == STATE_SNOOZED then
    snooze_until = 0
    state = bambu.printing() and STATE_WATCHING or STATE_IDLE
  end
  update_ui()
end)

-- ─────────────────────────────────────────────────────────────────────────────
--  Main tick
-- ─────────────────────────────────────────────────────────────────────────────
local tick_count = 0

sys.on_tick(function(_dt)
  local now     = sys.millis()
  local printing = bambu.printing()

  -- Expire snooze
  if state == STATE_SNOOZED and now >= snooze_until then
    state = printing and STATE_WATCHING or STATE_IDLE
    lost_since = 0
  end

  -- Only scan on interval (scans block for SCAN_MS)
  if now >= scan_due then
    scan_due = now + SCAN_INTERVAL
    local present, rssi = do_scan()

    if not printing then
      state = STATE_IDLE

    elseif state == STATE_SNOOZED then
      -- do nothing, let snooze expire naturally

    elseif present then
      state = STATE_WATCHING

    elseif lost_since > 0 and (now - lost_since) > ALERT_GRACE then
      if state ~= STATE_ALERT then
        sys.beep()  -- beep on alert transition
      end
      state = STATE_ALERT

    else
      state = STATE_SEARCHING
    end
  end

  update_ui()
end)

-- Initial draw
update_ui()
