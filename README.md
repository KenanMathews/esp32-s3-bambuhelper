# BambuHelper

Dedicated Bambu Lab printer monitor built with ESP32-S3 Super Mini and a 1.54" 240x240 color TFT display (ST7789).

Connects to your printer via MQTT over TLS and displays a real-time dashboard with arc gauges, animations, and live stats.

### Supported Printers

| Connection Mode | Printers | How it connects |
|---|---|---|
| **LAN Direct** | P1P, P1S, X1, X1C, X1E, A1, A1 Mini | Local MQTT via printer IP + LAN access code |
| **Bambu Cloud (All printers)** | Any Bambu printer | Cloud MQTT via access token - no LAN mode needed |

> **Tip:** Use "Bambu Cloud (All printers)" if you don't want to enable LAN mode on your printer (e.g. to keep Bambu Handy working), if your ESP32 is on a different network than the printer, or if your printer only supports cloud mode (H2C, H2D, H2S, P2S).

### Cloud Mode Security Notice

When using Bambu Cloud, BambuHelper connects through Bambu Lab's cloud MQTT service. Here's what you need to know:

- **No credentials are stored** - BambuHelper never asks for your email or password. You extract an access token from your browser and paste it into the web interface.
- **Only the access token is stored** in the ESP32's flash memory. This token expires after ~3 months, at which point you simply paste a new one.
- **Read-only access** - BambuHelper only reads printer status. It never sends commands or modifies printer settings.
- **Same approach as other community projects** - this is the same authentication method used by the [Home Assistant Bambu Lab integration](https://github.com/greghesp/ha-bambulab) (15,000+ users), [OctoPrint-Bambu](https://github.com/jneilliii/OctoPrint-BambuPrinter), and other trusted third-party tools.

## Screenshots

| Dashboard | Web Interface - Settings | Web Interface - Gauge Colors |
|---|---|---|
| ![Dashboard](img/interface1.jpg) | ![Settings](img/screen1.png) | ![Gauge Colors](img/screen2.png) |

## Features

- **Live dashboard** - progress arc, temperature gauges, fan speed, layer count, time remaining
- **H2-style LED progress bar** - full-width glowing bar inspired by Bambu H2 series
- **Anti-aliased arc gauges** - smooth nozzle and bed temperature arcs with color zones
- **Animations** - loading spinner, progress pulse, completion celebration
- **Web config portal** - dark-themed settings page for WiFi, network, printer, display, and power settings
- **Network configuration** - DHCP or static IP, with optional IP display at startup
- **Display auto-off** - configurable timeout after print completion, auto-off when printer is off
- **NVS persistence** - all settings survive reboots
- **Auto AP mode** - creates WiFi hotspot on first boot or when WiFi is lost
- **Smart redraw** - only redraws changed UI elements for smooth performance
- **Customizable gauge colors** - per-gauge arc/label/value colors with preset themes
- **Multi-printer support** - monitor up to 2 printers simultaneously with auto-rotating display
- **Smart rotation** - automatically shows the printing printer; cycles between both when both are printing
- **Physical button** - optional push button or TTP223 touch sensor to cycle printers and wake display
- **Exponential backoff** - reconnect attempts to offline printers gradually slow down to conserve resources

## Hardware

| Component | Specification |
|---|---|
| MCU | ESP32-S3 Super Mini |
| Display | 1.54" TFT SPI ST7789 (240x240) |
| Connection | SPI |

Display: 1.54": https://a.aliexpress.com/_EG9y7wc

ESP32-S3 SuperMini: https://a.aliexpress.com/_Eyk9GdA

Optional: TTP223 touch button or standard push button for multi-printer switching (auto printer switching works without button anyway, change settings in web interface)

Optional: Passive buzzer for print finish/error notifications: https://aliexpress.com/item/1005008825917787.html

Optional case seen on picture: https://makerworld.com/en/models/2501721

### Default Wiring

| Display Pin | ESP32-S3 GPIO |
|---|---|
| MOSI (SDA) | 11 |
| SCLK (SCL) | 12 |
| CS | 10 |
| DC | 9 |
| RST | 8 |
| BL | 13 |
| GND | GND |
| VCC | 3.3V |

Adjust pin assignments in `platformio.ini` build_flags to match your wiring.

Touch TTP223 button is optional, it is used to switch between printers, you may use standard push button and connect it between pin 4 and GND. Then pick correct button setting in web interface under Multi-Printer support

![wiring](img/wiring.png)

### Assembly Video

[![Assembly Video](https://img.youtube.com/vi/hsyamsU5UZE/maxresdefault.jpg)](https://youtu.be/hsyamsU5UZE)

## Flashing

1. Download the latest `BambuHelper-WebFlasher.bin` from [Releases](../../releases)
2. Open [ESP Web Flasher](https://espressif.github.io/esptool-js/) in Chrome or Edge
3. Connect your ESP32-S3 via USB
4. Click **Connect** and select your device
5. Set flash address to **0x0**
6. Select the downloaded `.bin` file
7. Click **Program**

## Setup

### Configuration Guide

[![Configuration Guide](https://img.youtube.com/vi/n2RdbeHTMz0/maxresdefault.jpg)](https://youtu.be/n2RdbeHTMz0)

1. **Flash** the firmware (see above)
2. **Connect** to the `BambuHelper-XXXX` WiFi network (password: `bambu1234`)
3. **Open** `192.168.4.1` in your browser
4. **Enter** your home WiFi credentials and **Save** - the device restarts and connects to your WiFi
5. **Note the IP address** shown on the ESP32 display after it connects to WiFi
6. **Open** that IP address in your browser to access the full web interface
7. **Configure your printer:**

   **LAN Direct** (P1P, P1S, X1, X1C, X1E, A1, A1 Mini):
   - Printer IP address (found in printer Settings > Network)
   - Serial number (see note below)
   - LAN access code (8 characters, from printer Settings > Network)

   **Bambu Cloud (All printers)**:
   - Get your Bambu Cloud access token from your browser (see [Getting a Cloud Token](#getting-a-cloud-token) below)
   - Paste the token into the web interface
   - Enter your printer's serial number (see note below)

   > **Important: Serial number is NOT the printer name.** The serial number is a 15-character code (e.g. `01P00A000000000`) found on the printer LCD under **Settings > Device > Serial Number**, or on the physical label on the back/bottom of the printer. Do not confuse it with the printer name shown in Bambu Studio (e.g. `3DP-01P-110`), which is a shortened version and will not work.

8. **Save Printer Settings** - the device connects to your printer

### Getting a Cloud Token

To use cloud mode, you need an access token from your Bambu Lab account. There are two ways to get it:

**Using browser DevTools (Chrome / Edge):**
1. Open https://bambulab.com and log in to your account
2. Press **F12** to open DevTools
3. Go to the **Application** tab (click `>>` if you don't see it)
4. In the left sidebar, expand **Cookies** → click `https://bambulab.com`
5. Find the row named `token` in the cookie list
6. Double-click the **Value** cell to select it, then **Ctrl+C** to copy
7. Paste the value into BambuHelper's "Access Token" field in the web interface

**Using browser DevTools (Firefox):**
1. Open https://bambulab.com and log in to your account
2. Press **F12** to open DevTools
3. Go to the **Storage** tab
4. In the left sidebar, expand **Cookies** → click `https://bambulab.com`
5. Find the row named `token`
6. Double-click the **Value** cell to select it, then **Ctrl+C** to copy
7. Paste the value into BambuHelper's "Access Token" field

**Using browser DevTools (Safari):**
1. Open https://bambulab.com and log in to your account
2. Open **Develop** → **Show Web Inspector** (enable the Develop menu first in Safari Preferences → Advanced)
3. Go to the **Storage** tab → **Cookies** → `bambulab.com`
4. Find and copy the `token` value
5. Paste it into BambuHelper's "Access Token" field

**Using the Python helper script (recommended):**
```bash
pip install curl_cffi
python tools/get_token.py
```
The script will prompt for your email, password, and 2FA code, then print the token. Copy and paste it into BambuHelper's web interface.

> **Note:** The token is valid for approximately 3 months. When it expires, the ESP32 will fail to connect - simply repeat the process above to get a fresh token and paste it in the web interface. Make sure to select the correct **Server Region** (US/EU/CN) to match your Bambu account's region.

## Web Interface

The built-in web interface (accessible at the device's IP address) provides the following settings:

### WiFi Settings
- **SSID** - your home WiFi network name
- **Password** - WiFi password

### Network
- **IP Assignment** - choose between DHCP (automatic) or Static IP
- **Static IP fields** (when static is selected):
  - IP Address
  - Gateway
  - Subnet Mask
  - DNS Server
- **Show IP at startup** - display the assigned IP on screen for 3 seconds after WiFi connects (on by default)

### Printer Settings
- **Connection Mode** - LAN Direct or Bambu Cloud (All printers)
- **LAN mode fields:**
  - Printer Name, Printer IP Address, Serial Number, LAN Access Code
- **Cloud mode fields:**
  - Server Region (US/EU/CN), Access Token, Printer Serial Number, Printer Name
- **Live Stats** - real-time nozzle/bed temp, progress, fan speed, and connection status

### Display
- **Brightness** - backlight level (10–255)
- **Screen Rotation** - 0°, 90°, 180°, 270°
- **Display off after print complete** - minutes to show the finish screen before turning off the display (0 = never turn off, default: 3 minutes)
- **Keep display always on** - override the timeout and never turn off
- **Show clock after print** - display a digital clock with date instead of turning off the screen (enabled by default)

### Gauge Colors
- **Theme presets** - Default, Mono Green, Neon, Warm, Ocean
- **Background color** - display background
- **Track color** - inactive arc background
- **Per-gauge colors** (arc, label, value) for:
  - Progress
  - Nozzle temperature
  - Bed temperature
  - Part fan
  - Aux fan
  - Chamber fan

### Other
- **Factory Reset** - erases all settings and restarts

## Dashboard Screens

| Screen | When |
|---|---|
| Splash | Boot (2 seconds) |
| AP Mode | First boot / no WiFi configured |
| Connecting WiFi | Attempting WiFi connection |
| WiFi Connected | Shows IP for 3s (if enabled) |
| Connecting Printer | WiFi connected, waiting for MQTT |
| Idle | Connected, printer not printing |
| Printing | Active print with full dashboard |
| Finished | Print complete with animation (auto-off after timeout) |
| Clock | After finish timeout (if enabled) - shows digital clock with date |
| Display Off | After finish timeout (if clock disabled) or printer powered off |

## Display Power Management

- After a print completes, the finish screen is shown for a configurable duration (default: 3 minutes), then either a digital clock is displayed or the screen turns off (configurable).
- When the printer is powered off or disconnected, the display stays in its current state (clock or off).
- When the printer comes back online or starts a new print, the display automatically wakes up.
- The "Keep display always on" option overrides the auto-off behavior.
- The "Show clock after print" option (enabled by default) shows time and date instead of turning off the display.

## Project Structure

```
include/
  config.h              Pin definitions, colors, constants
  bambu_state.h         Data structures (BambuState, PrinterConfig, ConnMode)
src/
  main.cpp              Setup/loop orchestrator
  settings.cpp          NVS persistence (WiFi, network, printer, display, power, cloud token)
  wifi_manager.cpp      WiFi STA + AP fallback, static IP support
  web_server.cpp        Config portal (HTML embedded, token management)
  bambu_mqtt.cpp        MQTT over TLS, delta merge (local + cloud broker)
  bambu_cloud.cpp       Bambu Cloud helpers (region URLs, JWT userId extraction)
  button.cpp            Physical button / touch sensor input
  display_ui.cpp        Screen state machine
  display_gauges.cpp    Arc gauges, progress bar, temp gauges
  display_anim.cpp      Animations (spinner, pulse, dots)
  clock_mode.cpp        Digital clock display (after print finishes)
  icons.h               16x16 pixel-art icons
tools/
  get_token.py          Python helper to get Bambu Cloud token on PC
```

## Requirements

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- **LAN mode:** Bambu Lab printer with LAN mode enabled, printer and ESP32 on the same local network
- **Cloud mode:** Bambu Lab account, ESP32 with internet access

## Multi-Printer Monitoring

BambuHelper supports monitoring up to 2 printers simultaneously via dual MQTT connections.

### Rotation Modes

| Mode | Behavior |
|---|---|
| **Smart** (default) | Shows the printing printer. If both are printing, cycles between them. If neither is printing, shows last active. |
| **Auto-rotate** | Cycles through all connected printers at a configurable interval (10s – 10min). |
| **Off** | Manually switch between printers using the physical button only. |

### Physical Button

An optional physical button can be connected to cycle between printers and wake the display from sleep.

| Type | Wiring | How it works |
|---|---|---|
| **Push button** | One pin to configured GPIO, other pin to GND | Active LOW with internal pull-up |
| **TTP223 touch sensor** | VCC→3.3V, GND→GND, SIG→configured GPIO | Active HIGH |

The button type and GPIO pin are configurable in the web interface (Multi-Printer section) — no recompilation needed.

### MQTT Reconnect Backoff

When a printer is physically powered off, BambuHelper uses exponential backoff to avoid wasting resources on repeated connection attempts:

| Phase | Attempts | Interval |
|---|---|---|
| Normal | First 5 | Every 10 seconds |
| Phase 2 | Next 10 | Every 60 seconds |
| Phase 3 | Beyond 15 | Every 120 seconds |

When the printer comes back online, the backoff resets to normal immediately.

## Troubleshooting

### WiFi won't connect / drops frequently

**SPI display cables near the ESP32 antenna can cause WiFi interference.** The ESP32-S3 Super Mini has a PCB antenna at one end of the board. If the SPI wires to the display run close to or over this antenna area, RF interference can prevent WiFi from connecting or cause frequent disconnections.

**Fix:** Route the display cables away from the antenna end of the ESP32-S3. Even 1–2 cm of separation can make a significant difference. If using a breadboard, ensure the wires don't loop back over the ESP32 module.

**Symptoms:**
- "Connecting to WiFi" screen appears briefly, then falls back to AP mode
- WiFi connects sometimes but drops after a few seconds
- Works fine when display is disconnected

**If WiFi issues persist**
Perform an antenna mod by soldering two individual goldpins to the antenna pads, as shown in the picture.

![wiring](img/antenamod.png)

### Printer shows "Connecting" but never connects

- **LAN Direct:** Make sure the printer and ESP32 are on the same network. Check that LAN mode is enabled on the printer and the access code is correct.
- **Bambu Cloud:** Verify the access token hasn't expired (~3 months validity). Re-extract from your browser and paste again. Check the server region matches your Bambu account.
- If a printer is physically powered off, reconnect attempts will gradually slow down (backoff). It will reconnect automatically when the printer comes back online.

### Display shows wrong printer / doesn't switch

- Check rotation mode in the web interface (Multi-Printer section). Smart mode only switches automatically when a printer is actively printing.
- Press the physical button (if configured) to manually cycle between printers.

## Lua SDK

BambuHelper apps are Lua 5.4 scripts that run on the device. The SDK exposes three modules — `ui`, `sys`, and `bambu` — plus an app manifest in header comments.

### App File Format

Every app is a `.lua` file with header comments that describe it to the app store:

```lua
-- @name        My App          (display name)
-- @color       0x07E0          (tile accent color, RGB565)
-- @sdk_min     2               (minimum SDK version required)
-- @version     1.0
-- @author      Your Name
-- @category    monitor         (monitor | tool | fun)
-- @description One-line description shown in the store.
```

### App Lifecycle

```lua
-- 1. Create your screen (auto-shown immediately)
local scr = ui.screen()

-- 2. Build widgets on the screen
local lbl = ui.label(scr, "Hello!", {align="center", font=24, color=0xFFFF})

-- 3. Register a tick callback (called ~30 times/second)
sys.on_tick(function(dt)
    -- dt = milliseconds since last tick
    ui.label_set(lbl, "Time: " .. sys.time())
end)

-- 4. Or use interval timers instead of on_tick
sys.every(1000, function()
    ui.label_set(lbl, bambu.job_name())
end)
```

---

### `ui.*` — Widget API

#### Screen & Containers

| Function | Description |
|---|---|
| `ui.screen()` | Create and immediately show a new full-screen LVGL screen. Returns a screen handle. |

#### Widgets

All widget constructors take `(parent, [text_or_value], opts_table)` and return a handle.

| Function | Opts keys | Description |
|---|---|---|
| `ui.label(parent, text, opts)` | `x y w align font color` | Text label |
| `ui.arc(parent, opts)` | `cx cy size value color track thickness start sweep` | Arc/ring gauge |
| `ui.rect(parent, opts)` | `x y w h color radius` | Filled rectangle |
| `ui.canvas(parent, w, h, x, y)` | — | Raw drawing surface |

`align` values: `"center"`, `"top_mid"`, `"bottom_mid"`, `"top_left"`, `"top_right"`, etc. (LVGL align names)

#### Update Functions

| Function | Description |
|---|---|
| `ui.label_set(handle, text)` | Update label text |
| `ui.label_color(handle, color)` | Update label color |
| `ui.arc_set(handle, value)` | Set arc value (0–100) |
| `ui.arc_color(handle, color)` | Set arc foreground color |
| `ui.rect_set(handle, color)` | Set rect fill color |
| `ui.rect_size(handle, w, h)` | Resize rect |
| `ui.delete(handle)` | Delete a widget |

#### Canvas Drawing

All canvas functions take the canvas handle as first argument. Colors are RGB565 integers.

| Function | Description |
|---|---|
| `ui.canvas_clear(canvas, color)` | Fill entire canvas with color |
| `ui.canvas_rect(canvas, x, y, w, h, color)` | Draw filled rectangle |
| `ui.canvas_circle(canvas, cx, cy, r, color, thickness)` | Draw circle outline |
| `ui.canvas_line(canvas, x1, y1, x2, y2, color, width)` | Draw line (color and width both required) |
| `ui.canvas_arc(canvas, cx, cy, r, start_deg, end_deg, color, thickness)` | Draw arc segment |

#### Animations

| Function | Description |
|---|---|
| `ui.anim_fade(handle, from, to, opts)` | Fade opacity from→to. `opts`: `{time=300, delay=0, repeat=false, bounce=false}` |
| `ui.anim_move(handle, x, y, opts)` | Animate widget to position (x, y). Same opts as above. |

#### Events

| Function | Description |
|---|---|
| `ui.on_tap(handle, fn)` | Call `fn()` when widget is tapped. Safe to call Lua — events are deferred and fired after the tick. |

#### Color Helper

| Function | Description |
|---|---|
| `ui.color(r, g, b)` | Convert 8-bit RGB values to an RGB565 integer. Values are clamped to 0–255. |

---

### `sys.*` — System API

| Function | Description |
|---|---|
| `sys.on_tick(fn)` | Register tick callback `fn(dt_ms)`. Called ~30×/second. Only one callback per app. |
| `sys.every(ms, fn)` | Register interval timer. Fires `fn()` every `ms` milliseconds independently of `on_tick`. Max 8 timers per app. |
| `sys.millis()` | Returns milliseconds since device boot. |
| `sys.time()` | Returns current time as a formatted string (e.g. `"14:32"`). |
| `sys.log(msg)` | Print a string to the serial console for debugging. |
| `sys.beep()` | Emit a short beep (if buzzer is connected). |
| `sys.exit()` | Stop the app and return to the launcher. |
| `sys.store_set(key, value)` | Persist a string value to flash (survives reboots and app restarts). |
| `sys.store_get(key)` | Read a persisted string value. Returns `nil` if key not found. |
| `sys.store_del(key)` | Delete a persisted key. |
| `sys.http_get(url)` | Blocking HTTP GET. Returns response body string or `nil` on failure. |
| `sys.json_parse(str)` | Parse a JSON string into a Lua table. Returns `nil` on parse error. |

---

### `bambu.*` — Printer Data API

All functions return live data from the connected Bambu Lab printer. Values are 0/`""` when the printer is idle or unreachable.

#### Print State

| Function | Returns | Description |
|---|---|---|
| `bambu.state()` | `string` | One of: `"IDLE"`, `"PRINTING"`, `"PAUSE"`, `"FAILED"`, `"FINISH"`, `"PREPARE"` |
| `bambu.printing()` | `boolean` | `true` when actively printing (not paused or idle) |
| `bambu.connected()` | `boolean` | `true` when printer is reachable over the network |
| `bambu.progress()` | `integer` 0–100 | Print progress percentage |
| `bambu.layer()` | `integer` | Current layer number |
| `bambu.total_layers()` | `integer` | Total layers in the job |
| `bambu.job_name()` | `string` | Current print job filename |
| `bambu.speed()` | `integer` | Speed level: 0=Silent, 1=Standard, 2=Sport, 3=Ludicrous |
| `bambu.remaining_mins()` | `integer` | Estimated minutes remaining |

#### Temperatures

| Function | Returns | Description |
|---|---|---|
| `bambu.nozzle_temp()` | `number` | Current nozzle temperature °C |
| `bambu.nozzle_target()` | `number` | Nozzle target setpoint °C |
| `bambu.bed_temp()` | `number` | Current bed temperature °C |
| `bambu.bed_target()` | `number` | Bed target setpoint °C |
| `bambu.chamber_temp()` | `number` | Chamber temperature °C |

#### Fans

| Function | Returns | Description |
|---|---|---|
| `bambu.fan_part()` | `integer` 0–100 | Part cooling fan speed % |
| `bambu.fan_aux()` | `integer` 0–100 | Auxiliary fan speed % |
| `bambu.fan_chamber()` | `integer` 0–100 | Chamber fan speed % |

#### AMS (Filament System)

| Function | Returns | Description |
|---|---|---|
| `bambu.ams_color(tray)` | `integer` | RGB565 color of tray 0–15. Returns `0` (not nil) when absent — always check `> 0` before using. |
| `bambu.ams_type(tray)` | `string` | Filament type string (e.g. `"PLA"`, `"PETG"`). `""` when absent. |
| `bambu.ams_active()` | `integer` | Active tray index 0–15, or `-1` if no AMS tray is active. |

#### Device

| Function | Returns | Description |
|---|---|---|
| `bambu.printer_name()` | `string` | User-configured printer name |

---

### Example App

```lua
-- @name Temp Watch
-- @color 0xF800
-- @sdk_min 2
-- @version 1.0
-- @author Example
-- @category monitor
-- @description Displays nozzle and bed temperatures with color-coded arcs.

local CLR_ORANGE = 0xFBE0
local CLR_BLUE   = 0x34DF
local CLR_GREEN  = 0x07E0
local CLR_TEXT   = 0xFFFF
local CLR_DARK   = 0x18E3

local scr = ui.screen()

local noz_arc = ui.arc(scr, {cx=120, cy=120, size=100, value=0,
    color=CLR_ORANGE, track=CLR_DARK, thickness=10})

local bed_arc = ui.arc(scr, {cx=120, cy=120, size=80, value=0,
    color=CLR_BLUE, track=CLR_DARK, thickness=8})

local noz_lbl = ui.label(scr, "N --°", {align="center", y=-10, font=20, color=CLR_ORANGE})
local bed_lbl = ui.label(scr, "B --°", {align="center", y=16,  font=16, color=CLR_BLUE})

sys.on_tick(function(_dt)
    local noz   = bambu.nozzle_temp()
    local noz_t = bambu.nozzle_target()
    local bed   = bambu.bed_temp()
    local bed_t = bambu.bed_target()

    local function temp_pct(cur, tgt)
        if tgt <= 0 then return 0 end
        return math.min(math.floor(cur / tgt * 100), 100)
    end

    ui.arc_set(noz_arc, temp_pct(noz, noz_t))
    ui.arc_set(bed_arc, temp_pct(bed, bed_t))

    local at_temp = noz_t > 0 and math.abs(noz - noz_t) < 5
    ui.arc_color(noz_arc, at_temp and CLR_GREEN or CLR_ORANGE)

    ui.label_set(noz_lbl, string.format("N %.0f°", noz))
    ui.label_set(bed_lbl, string.format("B %.0f°", bed))

    if bambu.state() == "FINISH" then sys.exit() end
end)
```

---

## Future Plans

- OTA firmware updates
- Multi-language support

## Credits

This project is based on [BambuHelper](https://github.com/Keralots/BambuHelper) by [Keralots](https://github.com/Keralots). The original project was built for an ESP32-S3 Super Mini with a 1.54" ST7789 TFT display using direct SPI rendering.

This fork adapts it for the **Waveshare ESP32-S3-Touch-LCD-1.28** (round 240×240 GC9A01A display) with the following major changes:
- Migrated rendering from direct SPI/TFT_eSPI to **LVGL 8**
- Added a **Lua scripting SDK** for user-installable apps
- Added an **app store** with install/delete over WiFi
- Added a **3×2 launcher grid** with live clock tile
- Added **LittleFS** app storage
- Added a **desktop simulator** for app development

The Bambu MQTT/cloud authentication and printer state logic is largely preserved from the original project.

## License

MIT
