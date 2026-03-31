-- @name Layer Timeline
-- @color 0x0228
-- @sdk_min 2
-- @version 1.0
-- @author BambuHelper
-- @category monitor
-- @description Bar chart of print speed per layer segment. Reveals slowdowns and layer shifts.

local CLR_GREEN  = 0x07E0
local CLR_ORANGE = 0xFBE0
local CLR_BLUE   = 0x34DF
local CLR_CYAN   = 0x07FF
local CLR_RED    = 0xF800
local CLR_YELLOW = 0xFFE0
local CLR_TEXT   = 0xFFFF
local CLR_DIM    = 0xC618
local CLR_DARK   = 0x18E3

-- Chart area: 200 wide x 100 tall, centred
local CHART_X  = 20
local CHART_Y  = 70
local CHART_W  = 200
local CHART_H  = 100
local MAX_BARS = 40   -- store last 40 layer samples

local bars     = {}   -- circular buffer of {layer, pct, speed} samples
local last_layer = -1
local sample_idx = 0

-- ── Build screen ───────────────────────────────────────────────────────────
local scr = ui.screen()

-- Title
ui.label(scr, "Layer Timeline", {align="top_mid", y=12, font=16, color=CLR_CYAN})

-- Job name
local job_lbl = ui.label(scr, bambu.job_name(), {align="top_mid", y=34, font=14, color=CLR_DIM, w=190})

-- Canvas for bar chart, positioned at CHART_X, CHART_Y
local canvas = ui.canvas(scr, CHART_W, CHART_H, CHART_X, CHART_Y)

-- Layer / progress labels
local layer_lbl = ui.label(scr, "L 0 / 0", {align="bottom_mid", y=-34, font=14, color=CLR_TEXT})
local prog_lbl  = ui.label(scr, "0%",       {align="bottom_mid", y=-16, font=14, color=CLR_GREEN})

-- ── Helpers ────────────────────────────────────────────────────────────────
local function bar_color(speed)
    if speed >= 3 then return CLR_ORANGE end  -- sport/ludicrous
    if speed >= 2 then return CLR_GREEN  end  -- standard
    return CLR_BLUE                            -- silent
end

local function redraw_chart()
    ui.canvas_clear(canvas, 0x0821)  -- very dark blue background

    local count = #bars
    if count == 0 then
        -- Draw "Waiting for print..." placeholder text using a center bar
        ui.canvas_rect(canvas, CHART_W//2 - 40, CHART_H//2 - 1, 80, 2, CLR_DIM)
        return
    end

    local bar_w = math.floor(CHART_W / MAX_BARS)
    if bar_w < 1 then bar_w = 1 end

    for i, s in ipairs(bars) do
        local bar_h = math.max(2, math.floor(s.pct / 100 * CHART_H))
        local x     = (i - 1) * bar_w
        local y     = CHART_H - bar_h
        ui.canvas_rect(canvas, x, y, bar_w - 1, bar_h, bar_color(s.speed))
    end

    -- Horizontal gridlines at 25%, 50%, 75%
    for _, pct in ipairs({25, 50, 75}) do
        local y = CHART_H - math.floor(pct / 100 * CHART_H)
        ui.canvas_line(canvas, 0, y, CHART_W, y, CLR_DARK, 1)
    end
end

-- ── Tick ───────────────────────────────────────────────────────────────────
sys.on_tick(function(_dt)
    local state = bambu.state()
    local layer = bambu.layer()
    local total = bambu.total_layers()
    local pct   = bambu.progress()
    local speed = bambu.speed()

    -- Sample on each new layer
    if layer ~= last_layer and layer > 0 then
        last_layer = layer

        if #bars >= MAX_BARS then
            table.remove(bars, 1)
        end
        table.insert(bars, {layer=layer, pct=pct, speed=speed})
        redraw_chart()
    end

    -- Update labels
    ui.label_set(job_lbl,   bambu.job_name())
    ui.label_set(layer_lbl, "L " .. layer .. " / " .. total)
    ui.label_set(prog_lbl,  pct .. "%")

    local prog_color = pct >= 90 and CLR_CYAN or (pct >= 50 and CLR_GREEN or CLR_YELLOW)
    ui.label_color(prog_lbl, prog_color)

    -- Exit when done or failed
    if state == "FINISH" or state == "FAILED" then
        sys.exit()
    end
end)
