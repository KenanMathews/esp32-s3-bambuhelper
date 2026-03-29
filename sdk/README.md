# BambuHelper SDK

Write Lua apps for the BambuHelper display — a 240x240 round screen on an ESP32-S3 that
shows live data from your Bambu Lab printer.

## Folder Structure

```
sdk/
  simulator/          Desktop simulator (macOS/Linux, SDL2 + LVGL + Lua)
    sdk_impl/         C implementation of bambu.*, ui.*, sys.* modules
    mock/             Editable mock data for printer state
    bin/              Compiled simulator binary (gitignored)
  demos/              Example apps
    basic-app/        Minimal status screen (progress arc, temps)
  stubs/              Lua stub files for IDE autocomplete
```

## Quick Start

```bash
# 1. Install dependencies (macOS)
brew install sdl2 lua

# 2. Build the simulator
cd sdk/simulator
mkdir build && cd build
cmake ..
cmake --build . -- -j4

# 3. Run a demo app
./bin/simulator ../../demos/basic-app/app.lua
```

## App Manifest Header

Every app script should begin with a header comment block:

```lua
-- @name        My App
-- @version     1.0
-- @sdk_min     1
-- @author      Your Name
-- @description One-line description of what this app does
-- @color       0x07E0
-- @category    display
```

| Field         | Description                                      |
|---------------|--------------------------------------------------|
| `@name`       | Display name shown in the app launcher           |
| `@version`    | Semantic version string                          |
| `@sdk_min`    | Minimum BambuHelper SDK version required         |
| `@author`     | Author name                                      |
| `@description`| Short description (one line)                     |
| `@color`      | Accent color as RGB565 hex (used in launcher UI) |
| `@category`   | One of: `display`, `control`, `utility`          |

## API Reference

### `bambu.*` — Printer Data

| Function              | Returns  | Description                                          |
|-----------------------|----------|------------------------------------------------------|
| `bambu.state()`       | string   | Gcode state: "PRINTING", "IDLE", "PAUSE", etc.       |
| `bambu.progress()`    | integer  | Print progress 0–100                                 |
| `bambu.nozzle_temp()` | number   | Current nozzle temperature (°C)                      |
| `bambu.bed_temp()`    | number   | Current bed temperature (°C)                         |
| `bambu.nozzle_target()` | number | Nozzle target temperature (°C)                       |
| `bambu.bed_target()`  | number   | Bed target temperature (°C)                          |
| `bambu.remaining_mins()` | integer | Estimated minutes remaining                        |
| `bambu.fan_part()`    | integer  | Part cooling fan speed 0–100                         |
| `bambu.fan_aux()`     | integer  | Auxiliary fan speed 0–100                            |
| `bambu.printer_name()` | string  | User-configured printer name                         |
| `bambu.connected()`   | boolean  | Whether the printer is reachable                     |
| `bambu.printing()`    | boolean  | True if a print is actively running                  |

### `ui.*` — Display Widgets

| Function                   | Returns      | Description                            |
|----------------------------|--------------|----------------------------------------|
| `ui.screen()`              | userdata     | Create a new blank screen              |
| `ui.label(scr, text, opts)`| userdata     | Create a text label                    |
| `ui.arc(scr, opts)`        | userdata     | Create a progress arc                  |
| `ui.rect(scr, opts)`       | userdata     | Create a filled rectangle              |
| `ui.show(scr)`             | —            | Make a screen visible                  |

**`ui.label` opts:** `align`, `x`, `y`, `color` (RGB565), `font` (14/16/20/28/40)

**`ui.arc` opts:** `cx`, `cy`, `value` (0–100), `min`, `max`, `color` (RGB565), `size`, `thickness`

**`ui.rect` opts:** `x`, `y`, `w`, `h`, `color` (RGB565), `radius`

**Alignment strings:** `"top_mid"`, `"top_left"`, `"top_right"`, `"center"`, `"bottom_mid"`, `"bottom_left"`, `"bottom_right"`

### `sys.*` — System

| Function              | Returns  | Description                              |
|-----------------------|----------|------------------------------------------|
| `sys.millis()`        | integer  | Milliseconds since startup               |
| `sys.log(msg)`        | —        | Print a message to the debug log         |
| `sys.beep(freq, ms)`  | —        | Play a tone (no-op in simulator)         |
| `sys.exit()`          | —        | Gracefully exit the app                  |
| `sys.sdk_version()`   | integer  | Returns the SDK version integer (1)      |

## IDE Autocomplete

The `stubs/` folder contains EmmyLua annotation files for use with VS Code and the
[Lua Language Server](https://marketplace.visualstudio.com/items?itemName=sumneko.lua).

Add this to `.vscode/settings.json` in your app folder:

```json
{
  "Lua.workspace.library": ["../../stubs"]
}
```
