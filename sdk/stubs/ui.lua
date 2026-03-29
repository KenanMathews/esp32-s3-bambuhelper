---@meta

---BambuHelper UI Module
---Create LVGL-based UI elements on the 240x240 round display.
---All widget handles are lightuserdata wrapping lv_obj_t pointers.
local ui = {}

---Create a new blank screen. The screen is not shown until ui.show() is called.
---@return userdata screen LVGL screen object handle
function ui.screen() end

---Create a text label on a screen.
---@param scr userdata Screen returned by ui.screen()
---@param text string Text to display
---@param opts? {align?: string, x?: number, y?: number, color?: number, font?: number} Options table
---  - align: Alignment string — one of "top_mid", "top_left", "top_right",
---           "center", "bottom_mid", "bottom_left", "bottom_right". Default: "center"
---  - x: Horizontal offset in pixels from the alignment point. Default: 0
---  - y: Vertical offset in pixels from the alignment point. Default: 0
---  - color: Text colour as RGB565 integer (e.g. 0xFFFF for white). Default: theme colour
---  - font: Font size — one of 14, 16, 20, 28, 40. Default: 14
---@return userdata label LVGL label object handle
function ui.label(scr, text, opts) end

---Create a progress arc on a screen.
---@param scr userdata Screen returned by ui.screen()
---@param opts {cx: number, cy: number, value?: number, min?: number, max?: number, color?: number, size?: number, thickness?: number} Options table
---  - cx: Centre x position in pixels
---  - cy: Centre y position in pixels
---  - value: Current value within [min, max]. Default: 0
---  - min: Range minimum. Default: 0
---  - max: Range maximum. Default: 100
---  - color: Indicator colour as RGB565 integer. Default: 0x07E0 (green)
---  - size: Outer diameter in pixels. Default: 80
---  - thickness: Arc line width in pixels. Default: 6
---@return userdata arc LVGL arc object handle
function ui.arc(scr, opts) end

---Create a filled rectangle on a screen.
---@param scr userdata Screen returned by ui.screen()
---@param opts {x: number, y: number, w: number, h: number, color?: number, radius?: number} Options table
---  - x: Left edge position in pixels
---  - y: Top edge position in pixels
---  - w: Width in pixels
---  - h: Height in pixels
---  - color: Fill colour as RGB565 integer. Default: mid-grey
---  - radius: Corner radius in pixels. Default: 4
---@return userdata rect LVGL object handle
function ui.rect(scr, opts) end

---Make a screen visible, replacing whatever is currently displayed.
---@param scr userdata Screen returned by ui.screen()
function ui.show(scr) end

return ui
