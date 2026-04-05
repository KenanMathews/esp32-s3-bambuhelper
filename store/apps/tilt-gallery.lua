-- @name Tilt Gallery
-- @color 0x34DF
-- @sdk_min 2
-- @version 1.0
-- @author BambuHelper
-- @category monitor
-- @description Swipe through 5 print stat cards by tilting the device left/right. Tap to go back to overview.

local CLR_BG     = 0x0000
local CLR_TEXT   = 0xFFFF
local CLR_DIM    = 0x8410
local CLR_CYAN   = 0x07FF
local CLR_GREEN  = 0x07E0
local CLR_ORANGE = 0xFBE0
local CLR_BLUE   = 0x34DF
local CLR_RED    = 0xF800
local CLR_GOLD   = 0xFEA0
local CLR_YELLOW = 0xFFE0

local CX, CY = 120, 120
local fi      = math.floor

-- ── Card definitions ──────────────────────────────────────────────────────────
-- Each card: { title, color, draw(canvas) }
local CARDS = {
  {
    id    = "progress",
    title = "Progress",
    color = CLR_GREEN,
  },
  {
    id    = "temps",
    title = "Temperatures",
    color = CLR_ORANGE,
  },
  {
    id    = "fans",
    title = "Fans",
    color = CLR_CYAN,
  },
  {
    id    = "ams",
    title = "AMS Trays",
    color = CLR_GOLD,
  },
  {
    id    = "eta",
    title = "Time Left",
    color = CLR_BLUE,
  },
}

local NUM_CARDS = #CARDS

-- ── State ─────────────────────────────────────────────────────────────────────
local current     = 1          -- active card index
local overview    = false      -- show dot-strip overview
local tilt_lock   = false      -- debounce tilt switches
local last_roll   = 0
local TILT_THRESH = 25.0       -- degrees to trigger card change
local LOCK_MS     = 600        -- ms before next tilt allowed
local lock_until  = 0

-- ── Screen ────────────────────────────────────────────────────────────────────
local scr    = ui.screen()
local canvas = ui.canvas(scr, 240, 240, 0, 0)

-- ── Draw helpers ──────────────────────────────────────────────────────────────
local function arc_pct(pct, clr, thick)
  thick = thick or 14
  -- background track
  ui.canvas_arc(canvas, CX, CY, 100, 0, 360, CLR_DIM, thick)
  if pct > 0 then
    local end_deg = fi(pct / 100 * 360)
    ui.canvas_arc(canvas, CX, CY, 100, 0, end_deg, clr, thick)
  end
end

local function center_label(text, color, y_off, size)
  -- draw text roughly centered (canvas has no text, so we approximate with arcs)
  -- We use ui.label instead, layered over canvas
  return {text=text, color=color, y_off=y_off, size=size}
end

-- Dot strip — shows which card is active
local function draw_dots(active_idx)
  local spacing = 16
  local total_w = (NUM_CARDS - 1) * spacing
  local start_x = CX - fi(total_w / 2)
  for i = 1, NUM_CARDS do
    local x = start_x + (i - 1) * spacing
    local clr = (i == active_idx) and CLR_TEXT or CLR_DIM
    local r   = (i == active_idx) and 5 or 3
    ui.canvas_circle(canvas, x, 222, r, clr)
  end
end

-- ── Card renderers ────────────────────────────────────────────────────────────
local lbl_big   = ui.label(scr, "",  {align="center",     y=0,   font=28, color=CLR_TEXT})
local lbl_sub   = ui.label(scr, "",  {align="center",     y=30,  font=16, color=CLR_DIM})
local lbl_title = ui.label(scr, "",  {align="top_mid",    y=14,  font=16, color=CLR_TEXT})
local lbl_hint  = ui.label(scr, "",  {align="bottom_mid", y=-30, font=14, color=CLR_DIM})

-- Extra labels for temps/fans (shown/hidden by clearing text)
local lbl_row1  = ui.label(scr, "", {x=30,  y=90,  font=16, color=CLR_ORANGE})
local lbl_row2  = ui.label(scr, "", {x=30,  y=115, font=16, color=CLR_BLUE})
local lbl_row3  = ui.label(scr, "", {x=30,  y=140, font=16, color=CLR_CYAN})
local lbl_row4  = ui.label(scr, "", {x=30,  y=165, font=16, color=CLR_GOLD})
local lbl_val1  = ui.label(scr, "", {x=145, y=90,  font=16, color=CLR_TEXT})
local lbl_val2  = ui.label(scr, "", {x=145, y=115, font=16, color=CLR_TEXT})
local lbl_val3  = ui.label(scr, "", {x=145, y=140, font=16, color=CLR_TEXT})
local lbl_val4  = ui.label(scr, "", {x=145, y=165, font=16, color=CLR_TEXT})

local function clear_rows()
  ui.label_set(lbl_row1, "") ui.label_set(lbl_val1, "")
  ui.label_set(lbl_row2, "") ui.label_set(lbl_val2, "")
  ui.label_set(lbl_row3, "") ui.label_set(lbl_val3, "")
  ui.label_set(lbl_row4, "") ui.label_set(lbl_val4, "")
  ui.label_set(lbl_big,  "")
  ui.label_set(lbl_sub,  "")
end

local function draw_card(idx)
  local card = CARDS[idx]
  ui.canvas_clear(canvas, CLR_BG)
  clear_rows()

  ui.label_set(lbl_title, card.title)
  ui.label_color(lbl_title, card.color)
  ui.label_set(lbl_hint, "< tilt >")

  if card.id == "progress" then
    -- Big arc + percentage
    local pct = bambu.progress()
    local layer = bambu.layer()
    local total = bambu.total_layers()
    arc_pct(pct, card.color)
    ui.label_set(lbl_big, pct .. "%")
    ui.label_color(lbl_big, card.color)
    ui.label_set(lbl_sub, "L " .. layer .. " / " .. total)
    draw_dots(idx)

  elseif card.id == "temps" then
    -- 2 arcs: nozzle outer, bed inner
    local noz  = fi(bambu.nozzle_temp())
    local nozt = fi(bambu.nozzle_target())
    local bed  = fi(bambu.bed_temp())
    local bedt = fi(bambu.bed_target())
    local noz_pct = nozt > 0 and fi(noz / nozt * 100) or 0
    local bed_pct = bedt > 0 and fi(bed / bedt * 100) or 0
    ui.canvas_arc(canvas, CX, CY, 108, 0, 360, CLR_DIM, 10)
    ui.canvas_arc(canvas, CX, CY, 108, 0, fi(noz_pct / 100 * 360), CLR_ORANGE, 10)
    ui.canvas_arc(canvas, CX, CY,  88, 0, 360, CLR_DIM, 10)
    ui.canvas_arc(canvas, CX, CY,  88, 0, fi(bed_pct / 100 * 360), CLR_BLUE, 10)
    ui.label_set(lbl_row1, "Nozzle")  ui.label_set(lbl_val1, noz .. " / " .. nozt .. "°")
    ui.label_set(lbl_row2, "Bed")     ui.label_set(lbl_val2, bed .. " / " .. bedt .. "°")
    local ch = fi(bambu.chamber_temp())
    ui.label_set(lbl_row3, "Chamber") ui.label_set(lbl_val3, ch .. "°")
    draw_dots(idx)

  elseif card.id == "fans" then
    -- 3 fan arcs stacked
    local pf = bambu.fan_part()
    local af = bambu.fan_aux()
    local cf = bambu.fan_chamber()
    ui.canvas_arc(canvas, CX, CY, 108, 0, 360, CLR_DIM, 9)
    ui.canvas_arc(canvas, CX, CY, 108, 0, fi(pf / 100 * 360), CLR_CYAN, 9)
    ui.canvas_arc(canvas, CX, CY,  89, 0, 360, CLR_DIM, 9)
    ui.canvas_arc(canvas, CX, CY,  89, 0, fi(af / 100 * 360), CLR_BLUE, 9)
    ui.canvas_arc(canvas, CX, CY,  70, 0, 360, CLR_DIM, 9)
    ui.canvas_arc(canvas, CX, CY,  70, 0, fi(cf / 100 * 360), CLR_GREEN, 9)
    ui.label_set(lbl_row1, "Part")    ui.label_set(lbl_val1, pf .. "%")
    ui.label_set(lbl_row2, "Aux")     ui.label_set(lbl_val2, af .. "%")
    ui.label_set(lbl_row3, "Chamber") ui.label_set(lbl_val3, cf .. "%")
    draw_dots(idx)

  elseif card.id == "ams" then
    -- 4 color swatches in a 2x2 grid
    local active = bambu.ams_active()
    local positions = {{55,95},{135,95},{55,145},{135,145}}
    for i = 1, 4 do
      local clr  = bambu.ams_color(i - 1)
      local typ  = bambu.ams_type(i - 1)
      local px, py = positions[i][1], positions[i][2]
      local border = (i - 1 == active) and CLR_TEXT or CLR_DIM
      -- outer border
      ui.canvas_rect(canvas, px - 2, py - 2, 54, 34, border)
      -- fill
      if clr and clr ~= 0 then
        ui.canvas_rect(canvas, px, py, 50, 30, clr)
      else
        ui.canvas_rect(canvas, px, py, 50, 30, CLR_DIM)
      end
    end
    local active_type = bambu.ams_type(active) or "—"
    ui.label_set(lbl_big, "")
    ui.label_set(lbl_sub, active_type)
    draw_dots(idx)

  elseif card.id == "eta" then
    -- Countdown display
    local mins = bambu.remaining_mins()
    local h    = fi(mins / 60)
    local m    = mins % 60
    local eta_str
    if mins <= 0 then
      eta_str = "Done"
    elseif h > 0 then
      eta_str = h .. "h " .. m .. "m"
    else
      eta_str = m .. "m"
    end
    arc_pct(bambu.progress(), card.color)
    ui.label_set(lbl_big, eta_str)
    ui.label_color(lbl_big, card.color)
    ui.label_set(lbl_sub, bambu.job_name())
    draw_dots(idx)
  end
end

-- ── Initial draw ──────────────────────────────────────────────────────────────
draw_card(current)

-- ── Tap: cycle forward ────────────────────────────────────────────────────────
ui.on_tap(canvas, function()
  current = (current % NUM_CARDS) + 1
  draw_card(current)
end)

-- ── Tick: tilt detection ──────────────────────────────────────────────────────
sys.on_tick(function(_dt)
  -- Tilt detection via IMU
  local t    = imu.tilt()
  local roll = t and t.roll or 0
  local now  = sys.millis()

  if now >= lock_until then
    if roll > TILT_THRESH and last_roll <= TILT_THRESH then
      -- Tilted right → next card
      current = (current % NUM_CARDS) + 1
      draw_card(current)
      lock_until = now + LOCK_MS
    elseif roll < -TILT_THRESH and last_roll >= -TILT_THRESH then
      -- Tilted left → previous card
      current = ((current - 2 + NUM_CARDS) % NUM_CARDS) + 1
      draw_card(current)
      lock_until = now + LOCK_MS
    end
  end
  last_roll = roll

  -- Refresh data every tick (labels are cheap)
  draw_card(current)
end)
