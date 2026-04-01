---@meta

---BambuHelper UI Module
---Create LVGL-based UI elements on the 240x240 round display.
---All widget handles are lightuserdata wrapping lv_obj_t pointers.
local ui = {}

-- ---------------------------------------------------------------------------
--  Screen
-- ---------------------------------------------------------------------------

---Create a new blank screen and immediately show it on the display.
---Add all widgets right after calling this — they go onto the live screen.
---@return userdata screen LVGL screen object handle
function ui.screen() end

-- ---------------------------------------------------------------------------
--  Label
-- ---------------------------------------------------------------------------

---Create a text label on a screen.
---@param scr userdata Screen returned by ui.screen()
---@param text string Text to display
---@param opts? {align?: string, x?: number, y?: number, color?: number, font?: number, w?: number} Options table
---  - align: "center" | "top_mid" | "top_left" | "top_right" | "bottom_mid" | "bottom_left" | "bottom_right" | "left_mid" | "right_mid"
---  - x: Horizontal offset in pixels from the alignment point. Default: 0
---  - y: Vertical offset in pixels from the alignment point. Default: 0
---  - color: Text colour as RGB565 integer. Default: theme white
---  - font: Font size — one of 14, 16, 20, 28, 40. Default: 16
---  - w: Fixed width in pixels; clips text beyond this width. Default: auto
---@return userdata label LVGL label handle
function ui.label(scr, text, opts) end

---Update label text in place.
---@param label userdata Label handle from ui.label()
---@param text string New text
function ui.label_set(label, text) end

---Change label text colour.
---@param label userdata Label handle from ui.label()
---@param color number RGB565 colour integer
function ui.label_color(label, color) end

-- ---------------------------------------------------------------------------
--  Arc (progress ring widget)
-- ---------------------------------------------------------------------------

---Create a progress arc on a screen.
---@param scr userdata Screen returned by ui.screen()
---@param opts {cx: number, cy: number, value?: number, size?: number, color?: number, track?: number, thickness?: number, start?: number, sweep?: number} Options table
---  - cx: Centre x position in pixels
---  - cy: Centre y position in pixels
---  - value: Initial value 0–100. Default: 0
---  - size: Radius in pixels (arc spans size*2 × size*2). Default: 80
---  - color: Indicator colour as RGB565. Default: 0x07E0 (green)
---  - track: Background track colour as RGB565. Default: dim grey
---  - thickness: Arc line width in pixels. Default: 8
---  - start: Start angle in degrees (0 = right, clockwise). Default: 135
---  - sweep: Arc span in degrees. Default: 270
---@return userdata arc LVGL arc handle
function ui.arc(scr, opts) end

---Update arc value.
---@param arc userdata Arc handle from ui.arc()
---@param value number New value 0–100
function ui.arc_set(arc, value) end

---Change arc indicator colour.
---@param arc userdata Arc handle from ui.arc()
---@param color number RGB565 colour integer
function ui.arc_color(arc, color) end

-- ---------------------------------------------------------------------------
--  Rectangle
-- ---------------------------------------------------------------------------

---Create a filled rectangle on a screen. Position is specified by centre point.
---@param scr userdata Screen returned by ui.screen()
---@param opts {cx?: number, cy?: number, w: number, h: number, color?: number, radius?: number, border?: boolean, border_color?: number} Options table
---  - cx: Centre x in pixels. Default: 120
---  - cy: Centre y in pixels. Default: 120
---  - w: Width in pixels
---  - h: Height in pixels
---  - color: Fill colour as RGB565. Default: mid-grey
---  - radius: Corner radius in pixels. Default: 0
---  - border: Draw a 1px border. Default: false
---  - border_color: Border colour as RGB565. Default: dim white
---@return userdata rect LVGL object handle
function ui.rect(scr, opts) end

---Change rectangle fill colour.
---@param rect userdata Rect handle from ui.rect()
---@param color number RGB565 colour integer
function ui.rect_set(rect, color) end

---Resize a rectangle.
---@param rect userdata Rect handle from ui.rect()
---@param w number New width in pixels
---@param h number New height in pixels
function ui.rect_size(rect, w, h) end

-- ---------------------------------------------------------------------------
--  Animation
-- ---------------------------------------------------------------------------

---Fade a widget's opacity from `from` to `to`.
---@param widget userdata Any widget handle
---@param from number Starting opacity 0–255 (0 = transparent, 255 = opaque)
---@param to number Target opacity 0–255
---@param opts? {time?: number, delay?: number, repeat?: boolean, bounce?: boolean, easing?: string} Options table
---  - time: Duration in ms. Default: 300
---  - delay: Delay before start in ms. Default: 0
---  - repeat: Loop indefinitely. Default: false
---  - bounce: Reverse animation after reaching `to`. Default: false
---  - easing: "linear"|"ease_in"|"ease_out"|"ease_in_out"|"overshoot"|"bounce"|"step". Default: "ease_in_out"
function ui.anim_fade(widget, from, to, opts) end

---Animate a widget to an absolute position.
---@param widget userdata Any widget handle
---@param x number Target x position in pixels
---@param y number Target y position in pixels
---@param opts? {time?: number, delay?: number, repeat?: boolean, bounce?: boolean, easing?: string} Options table
---  - time: Duration in ms. Default: 300
---  - delay: Delay before start in ms. Default: 0
---  - repeat: Loop indefinitely. Default: false
---  - bounce: Reverse animation after reaching target. Default: false
---  - easing: "linear"|"ease_in"|"ease_out"|"ease_in_out"|"overshoot"|"bounce"|"step". Default: "ease_in_out"
function ui.anim_move(widget, x, y, opts) end

---Stop all running animations on a widget immediately.
---@param widget userdata Any widget handle
function ui.anim_stop(widget) end

---Animate an arc widget's value from `from` to `to` (range 0–100).
---@param arc userdata Arc handle from ui.arc()
---@param from integer Start value 0–100
---@param to integer End value 0–100
---@param opts? {time?: number, delay?: number, repeat?: boolean, bounce?: boolean, easing?: string} Options table
---  - time: Duration in ms. Default: 500
---  - delay: Delay before start in ms. Default: 0
---  - repeat: Loop indefinitely. Default: false
---  - bounce: Reverse animation after reaching `to`. Default: false
---  - easing: "linear"|"ease_in"|"ease_out"|"ease_in_out"|"overshoot"|"bounce"|"step". Default: "ease_in_out"
function ui.anim_arc(arc, from, to, opts) end

---Animate a widget's size from its current dimensions to (w, h).
---@param widget userdata Any widget handle
---@param w integer Target width in pixels
---@param h integer Target height in pixels
---@param opts? {time?: number, delay?: number, repeat?: boolean, bounce?: boolean, easing?: string} Options table
---  - time: Duration in ms. Default: 300
---  - delay: Delay before start in ms. Default: 0
---  - repeat: Loop indefinitely. Default: false
---  - bounce: Reverse after reaching target size. Default: false
---  - easing: "linear"|"ease_in"|"ease_out"|"ease_in_out"|"overshoot"|"bounce"|"step". Default: "ease_in_out"
function ui.anim_size(widget, w, h, opts) end

-- ---------------------------------------------------------------------------
--  Canvas (raw pixel drawing, PSRAM-backed)
-- ---------------------------------------------------------------------------

---Create a canvas widget backed by a PSRAM pixel buffer.
---Returns nil if PSRAM allocation fails.
---All canvas_* functions draw into this buffer; the display updates automatically.
---@param scr userdata Screen returned by ui.screen()
---@param w number Canvas width in pixels
---@param h number Canvas height in pixels
---@param x? number Canvas x position. Default: 0
---@param y? number Canvas y position. Default: 0
---@return userdata|nil canvas LVGL canvas handle, or nil on allocation failure
function ui.canvas(scr, w, h, x, y) end

---Draw a line on a canvas.
---@param canvas userdata Canvas handle from ui.canvas()
---@param x1 number Start x in pixels (relative to canvas origin)
---@param y1 number Start y in pixels
---@param x2 number End x in pixels
---@param y2 number End y in pixels
---@param color number Line colour as RGB565
---@param width number Line width in pixels (minimum 1)
function ui.canvas_line(canvas, x1, y1, x2, y2, color, width) end

---Draw a filled rectangle on a canvas.
---@param canvas userdata Canvas handle from ui.canvas()
---@param x number Left edge in pixels
---@param y number Top edge in pixels
---@param w number Width in pixels
---@param h number Height in pixels
---@param color? number Fill colour as RGB565. Default: mid-grey
---@param radius? number Corner radius in pixels. Default: 0
function ui.canvas_rect(canvas, x, y, w, h, color, radius) end

---Draw a circle outline on a canvas.
---@param canvas userdata Canvas handle from ui.canvas()
---@param cx number Centre x in pixels
---@param cy number Centre y in pixels
---@param r number Radius in pixels
---@param color? number Line colour as RGB565. Default: white
---@param thickness? number Line width in pixels. Default: 1
function ui.canvas_circle(canvas, cx, cy, r, color, thickness) end

---Draw an arc on a canvas. Angles are in degrees: 0° = 3 o'clock, clockwise.
---@param canvas userdata Canvas handle from ui.canvas()
---@param cx number Centre x in pixels
---@param cy number Centre y in pixels
---@param r number Radius in pixels
---@param a1 number Start angle in degrees
---@param a2 number End angle in degrees
---@param color? number Line colour as RGB565. Default: white
---@param thickness? number Line width in pixels. Default: 1
function ui.canvas_arc(canvas, cx, cy, r, a1, a2, color, thickness) end

---Draw connected line segments from a flat table of x,y pairs.
---@param canvas userdata Canvas handle from ui.canvas()
---@param points table Flat table of x,y pairs: {x1,y1, x2,y2, ...} — minimum 2 points (4 values)
---@param color? number RGB565 colour. Default: white
---@param width? number Line width in pixels. Default: 1
function ui.canvas_polyline(canvas, points, color, width) end

---Draw a cubic Bezier curve on a canvas.
---@param canvas userdata Canvas handle from ui.canvas()
---@param x0 number Start x
---@param y0 number Start y
---@param cx1 number Control point 1 x
---@param cy1 number Control point 1 y
---@param cx2 number Control point 2 x
---@param cy2 number Control point 2 y
---@param x1 number End x
---@param y1 number End y
---@param color? number RGB565 colour. Default: white
---@param width? number Line width in pixels. Default: 1
---@param steps? number Smoothness — number of segments 2–64. Default: 20
function ui.canvas_bezier(canvas, x0, y0, cx1, cy1, cx2, cy2, x1, y1, color, width, steps) end

---Fill the entire canvas with a solid colour.
---@param canvas userdata Canvas handle from ui.canvas()
---@param color? number Fill colour as RGB565. Default: background colour
function ui.canvas_clear(canvas, color) end

-- ---------------------------------------------------------------------------
--  Touch input
-- ---------------------------------------------------------------------------

---Register a tap callback for a widget.
---The callback fires from the tick loop (safe to call any ui.* or sys.* inside).
---Only one callback per widget; re-registering replaces the previous.
---Requires a sys.on_tick() loop to be running — taps are not delivered to static apps.
---Do not delete the widget from inside its own tap callback.
---@param widget userdata Any handle from ui.rect(), ui.label(), etc.
---@param fn fun() Callback with no arguments
function ui.on_tap(widget, fn) end

-- ---------------------------------------------------------------------------
--  Utilities
-- ---------------------------------------------------------------------------

---Convert 8-bit RGB components to an RGB565 integer for use with any color parameter.
---Input values are clamped to 0–255.
---@param r number Red component 0–255
---@param g number Green component 0–255
---@param b number Blue component 0–255
---@return number rgb565 Packed RGB565 colour integer
function ui.color(r, g, b) end

---Delete a widget and free its LVGL resources.
---Do not use the handle after calling this.
---@param widget userdata Any widget handle
function ui.delete(widget) end

return ui
