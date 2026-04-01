# BambuHelper — Waveshare Round Display Fork

Dedicated Bambu Lab printer monitor for the **Waveshare ESP32-S3-Touch-LCD-1.28** (round 240×240 GC9A01A display).

Connects to your printer via MQTT over TLS and runs a **Lua scripting SDK** that lets you install, write, and run apps on the device — from animated print monitors to custom dashboards.

> This is a fork of [BambuHelper](https://github.com/Keralots/BambuHelper) by [Keralots](https://github.com/Keralots), adapted for the round display with a complete Lua app platform layered on top. See [What's different from upstream](#whats-different-from-upstream) for the full list of additions.

---

### Supported Printers

| Connection Mode | Printers | How it connects |
|---|---|---|
| **LAN Direct** | P1P, P1S, X1, X1C, X1E, A1, A1 Mini | Local MQTT via printer IP + LAN access code |
| **Bambu Cloud (All printers)** | Any Bambu printer | Cloud MQTT via access token — no LAN mode needed |

> **Tip:** Use "Bambu Cloud" if you don't want to enable LAN mode, if your ESP32 is on a different network, or if your printer only supports cloud mode (H2C, H2D, H2S, P2S).

### Cloud Mode Security Notice

- **No credentials stored** — BambuHelper never asks for your email or password. You extract an access token from your browser.
- **Only the access token** is stored in flash. It expires after ~3 months; paste a new one when it does.
- **Read-only** — BambuHelper only reads printer status; it never sends commands.
- Same authentication approach used by the [Home Assistant Bambu Lab integration](https://github.com/greghesp/ha-bambulab) and other trusted community tools.

---

## Hardware

| Component | Specification |
|---|---|
| MCU | Waveshare ESP32-S3-Touch-LCD-1.28 |
| Display | Round 1.28" GC9A01A TFT SPI (240×240, RGB565) |
| Touch | CST816S capacitive touch (I2C) |
| Flash | 16 MB |
| PSRAM | 2 MB |

Product page: https://www.waveshare.com/esp32-s3-touch-lcd-1.28.htm

Optional: Passive buzzer for print finish/error notifications (GPIO 5 by default)

Optional: Physical push button for manual printer switching (GPIO 4 by default)

### Default Pin Assignments

| Signal | GPIO |
|---|---|
| Display MOSI | 11 |
| Display SCLK | 10 |
| Display CS | 9 |
| Display DC | 8 |
| Display RST | 14 |
| Display BL | 2 |
| Touch SDA | 6 |
| Touch SCL | 7 |
| Touch INT | 5 |
| Touch RST | 13 |
| Button (optional) | 4 |
| Buzzer (optional) | 5 |

---

## What's Different from Upstream

This fork replaces the original direct-SPI rendering pipeline and static display code with a full Lua app platform:

| Area | Original BambuHelper | This Fork |
|---|---|---|
| **Display** | 1.54" rectangular ST7789 | Round 1.28" GC9A01A (240×240) |
| **Rendering** | Direct TFT_eSPI SPI writes | LVGL 8 widget framework |
| **UI** | Fixed C++ dashboard screens | Lua scripting SDK — fully scriptable |
| **Apps** | Single built-in dashboard | App store with install/delete over WiFi |
| **Launcher** | — | 3×2 grid launcher with live clock tile |
| **App storage** | — | LittleFS filesystem on flash |
| **Simulator** | — | Desktop macOS/Linux simulator for app development |
| **Animations** | Basic spinner/pulse | 9 state-specific animations in Mission Control |
| **Clock** | Digital clock with date | Tachometer-style clock arc with NTP sync |
| **State detection** | print/idle/finish | Full print stage detection (`print_stage`) |
| **MQTT buffer** | 16 KB | 40 KB (supports H2 series large pushall payloads) |
| **Stale state handling** | Reset on timeout | Recovery pushall with grace period before clearing state |
| **Offline state** | State persists across disconnect | Resets to IDLE immediately on disconnect |

---

## Features

- **Lua SDK** — write or install apps in Lua 5.4; `ui.*`, `sys.*`, `bambu.*` APIs
- **App store** — browse, install, and delete apps over WiFi from the web interface
- **3×2 launcher** — tap tiles to launch apps; live clock tile always visible
- **Mission Control** — built-in animated status app with 9 print-state animations
- **NTP clock** — accurate time via `sys.time()`, configurable timezone + 24h format
- **Multi-printer support** — monitor up to 2 printers simultaneously with auto-rotating display
- **Smart rotation** — automatically focuses the printing printer; cycles when both are printing
- **Web config portal** — dark-themed settings page for WiFi, printer, display, and power
- **NVS persistence** — all settings survive reboots
- **Auto AP mode** — creates a WiFi hotspot on first boot or when WiFi is lost
- **Physical button** — optional push button or TTP223 touch sensor to cycle printers and wake display
- **Buzzer notifications** — beeps on print finish or error (if buzzer connected)
- **Exponential backoff** — reconnect attempts to offline printers gradually slow down
- **Screensaver brightness** — configurable dim level for the screensaver / display-off state
- **Desktop simulator** — run and develop Lua apps on macOS without flashing hardware

---

## Mission Control App

The built-in **Mission Control** app (`store/apps/mission-control.lua`) is a unified animated print monitor. It reads `bambu.state()` and `bambu.print_stage()` each tick and drives one of nine state-specific animations:

| State / Stage | Animation |
|---|---|
| `IDLE` | Tachometer clock (NTP time, sweep second hand, arc hour/minute) |
| `RUNNING` (normal printing) | Spinning layer stack — concentric rings that build upward with nozzle indicator |
| `RUNNING` stage 1 (bed leveling) | Probing animation — scanning dot over a grid with green level bars |
| `RUNNING` stage 2/7 (heating) | Thermometer fill — rising column with temperature label |
| `RUNNING` stage 8/19 (flow calibration) | Wave oscilloscope — flow rate curve scrolling across the screen |
| `RUNNING` stage 13 (homing) | Crosshair targeting animation |
| `PREPARE` | Thermometer (heating before print starts) |
| `PAUSE` | Slow single orbit — a glowing dot circling the progress ring |
| `FINISH` | Celebration burst — radiating lines + checkmark |
| `FAILED` | Pulsing error cross with red glow |

Progress %, layer count, time remaining, and printer name labels update live. On FINISH, the elapsed time and layer count are saved to flash for the Print Report app. A buzzer beep fires on FINISH if a buzzer is connected.

---

## Screenshots

| Launcher | Mission Control — Printing | Mission Control — Idle Clock |
|---|---|---|
| *(round display)* | *(layer stack animation)* | *(tachometer clock)* |

---

## Flashing

1. Download the latest `BambuHelper-WebFlasher.bin` from [Releases](../../releases)
2. Open [ESP Web Flasher](https://espressif.github.io/esptool-js/) in Chrome or Edge
3. Connect your Waveshare board via USB
4. Click **Connect** and select your device
5. Set flash address to **0x0**
6. Select the downloaded `.bin` file and click **Program**

---

## Setup

1. **Flash** the firmware (see above)
2. **Connect** to the `BambuHelper-XXXX` WiFi network (password: `bambu1234`)
3. **Open** `192.168.4.1` in your browser
4. **Enter** your home WiFi credentials and **Save** — device restarts and connects
5. **Note the IP address** shown on the display after WiFi connects
6. **Open** that IP in your browser — full web interface
7. **Configure your printer:**

   **LAN Direct** (P1P, P1S, X1, X1C, X1E, A1, A1 Mini):
   - Printer IP, Serial Number, LAN Access Code (8 chars, from printer Settings > Network)

   **Bambu Cloud (All printers)**:
   - Server Region (US/EU/CN), Access Token, Printer Serial Number

   > **Serial number** is a 15-character code (e.g. `01P00A000000000`) found on the printer LCD under **Settings > Device > Serial Number** — not the shortened name shown in Bambu Studio.

8. **Save** — device connects to printer and launches the app grid

### Getting a Cloud Token

**Browser DevTools (Chrome / Edge):**
1. Go to https://bambulab.com and log in
2. Press F12 → **Application** tab → **Cookies** → `https://bambulab.com`
3. Find `token`, copy its value, paste into BambuHelper's "Access Token" field

**Browser DevTools (Firefox):**
1. F12 → **Storage** tab → **Cookies** → `https://bambulab.com`
2. Find `token`, copy and paste

**Python helper:**
```bash
pip install curl_cffi
python tools/get_token.py
```

The token is valid ~3 months. When it expires, repeat the above and paste the new token.

---

## Web Interface

### WiFi
- SSID, Password

### Network
- DHCP or Static IP (address, gateway, subnet, DNS)
- Show IP at startup (3 s splash)
- **Timezone** — select your region for NTP clock accuracy
- **24-hour format** — toggle for `sys.time()` and the idle clock

### Printer Settings
- Connection mode (LAN / Cloud), credentials, live stats panel

### Display
- Brightness (10–255)
- Screen rotation (0°, 90°, 180°, 270°)
- Display-off timeout after print complete
- Keep display always on
- Show clock after print

### Multi-Printer
- Up to 2 printer slots
- Rotation mode: Smart / Auto-rotate / Off
- Auto-rotate interval (10 s – 10 min)
- Physical button type (push / TTP223) and GPIO

### App Store
- Browse available apps with descriptions
- Install / delete apps
- Storage usage indicator

### Other
- Factory Reset

---

## App Store

Apps are Lua 5.4 scripts stored on LittleFS. The web interface shows all apps from the store index, lets you install them to the device, and delete them to free space.

### Built-in Apps

| App | Description |
|---|---|
| **Mission Control** | Animated printer monitor — state machine with 9 animations |
| **Print Report** | Shows last completed print: job name, duration, layers, temperatures |
| **Layer Timeline** | Bar chart of print speed per layer segment |
| **Crosshair** | Animated targeting crosshair during toolhead homing |

---

## Lua SDK

BambuHelper apps are Lua 5.4 scripts. The SDK exposes three modules: `ui`, `sys`, and `bambu`.

### App File Format

```lua
-- @name        My App
-- @color       0x07E0          (tile accent color, RGB565)
-- @sdk_min     2               (minimum SDK version)
-- @version     1.0
-- @author      Your Name
-- @category    monitor         (monitor | tool | fun)
-- @description One-line description shown in the store.
```

### App Lifecycle

```lua
local scr = ui.screen()
local lbl = ui.label(scr, "Hello!", {align="center", font=24, color=0xFFFF})

sys.on_tick(function(dt)
    -- dt = milliseconds since last tick (~30 fps)
    ui.label_set(lbl, bambu.job_name())
end)
```

---

### `ui.*` — Widget API

#### Widgets

| Function | Opts keys | Description |
|---|---|---|
| `ui.screen()` | — | Create and immediately show a full-screen LVGL screen |
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

Colors are RGB565 integers. Canvas handle is the first argument.

| Function | Description |
|---|---|
| `ui.canvas_clear(canvas, color)` | Fill entire canvas with color |
| `ui.canvas_rect(canvas, x, y, w, h, color)` | Draw filled rectangle |
| `ui.canvas_circle(canvas, cx, cy, r, color, thickness)` | Draw circle outline |
| `ui.canvas_line(canvas, x1, y1, x2, y2, color, width)` | Draw line |
| `ui.canvas_arc(canvas, cx, cy, r, start_deg, end_deg, color, thickness)` | Draw arc segment |

#### Animations

| Function | Description |
|---|---|
| `ui.anim_fade(handle, from, to, opts)` | Fade opacity (0–255) |
| `ui.anim_move(handle, x, y, opts)` | Animate to position |
| `ui.anim_arc(handle, from, to, opts)` | Animate arc value (0–100) |
| `ui.anim_size(handle, w, h, opts)` | Animate dimensions |
| `ui.anim_stop(handle)` | Cancel all running animations on a widget |

Animation `opts` table:

| Key | Default | Description |
|---|---|---|
| `time` | 300 (500 for `anim_arc`) | Duration in ms |
| `delay` | 0 | Delay before start in ms |
| `repeat` | false | Loop indefinitely |
| `bounce` | false | Reverse back after reaching target |
| `easing` | `"ease_in_out"` | `"linear"` `"ease_in"` `"ease_out"` `"ease_in_out"` `"overshoot"` `"bounce"` `"step"` |

#### Events & Colors

| Function | Description |
|---|---|
| `ui.on_tap(handle, fn)` | Call `fn()` when widget is tapped |
| `ui.color(r, g, b)` | Convert 8-bit RGB to RGB565 integer |

---

### `sys.*` — System API

| Function | Description |
|---|---|
| `sys.on_tick(fn)` | Register tick callback `fn(dt_ms)`. Called ~30×/second. One per app. |
| `sys.every(ms, fn)` | Register interval timer. Fires `fn()` every `ms` ms. Max 8 per app. |
| `sys.millis()` | Milliseconds since device boot |
| `sys.time()` | Returns a table: `{epoch, hour, min, sec, day, month, year, wday, synced}` |
| `sys.log(msg)` | Print to serial console |
| `sys.beep()` | Short buzzer beep (if buzzer connected) |
| `sys.exit()` | Stop the app and return to the launcher |
| `sys.store_set(key, value)` | Persist a string to flash (survives reboots) |
| `sys.store_get(key)` | Read a persisted string. Returns `nil` if not found. |
| `sys.store_del(key)` | Delete a persisted key |
| `sys.http_get(url [, timeout_ms])` | Blocking HTTP GET. Returns body string or `nil`. |
| `sys.json_parse(str)` | Parse JSON string into a Lua table. Returns `nil` on error. |

`sys.time()` uses NTP-synced time. Check `t.synced` before trusting `t.hour` etc. Timezone is set in the web interface Network settings.

---

### `bambu.*` — Printer Data API

All functions return live data from the connected printer. Values default to `0`/`""` when idle or unreachable.

#### Print State

| Function | Returns | Description |
|---|---|---|
| `bambu.state()` | `string` | One of: `"IDLE"` `"RUNNING"` `"PAUSE"` `"FAILED"` `"FINISH"` `"PREPARE"` |
| `bambu.print_stage()` | `integer` | Sub-state during RUNNING/PREPARE: `0`=normal printing `1`=bed leveling `2`/`7`=heating `8`/`19`=flow calibration `13`=homing |
| `bambu.printing()` | `boolean` | `true` when actively printing (not paused or idle) |
| `bambu.connected()` | `boolean` | `true` when printer is reachable |
| `bambu.progress()` | `integer` 0–100 | Print progress % |
| `bambu.layer()` | `integer` | Current layer number |
| `bambu.total_layers()` | `integer` | Total layer count |
| `bambu.job_name()` | `string` | Current print job filename |
| `bambu.speed()` | `integer` | Speed: 0=Silent 1=Standard 2=Sport 3=Ludicrous |
| `bambu.remaining_mins()` | `integer` | Estimated minutes remaining |

#### Temperatures

| Function | Returns | Description |
|---|---|---|
| `bambu.nozzle_temp()` | `number` | Current nozzle °C |
| `bambu.nozzle_target()` | `number` | Nozzle target °C |
| `bambu.bed_temp()` | `number` | Current bed °C |
| `bambu.bed_target()` | `number` | Bed target °C |
| `bambu.chamber_temp()` | `number` | Chamber °C |

#### Fans

| Function | Returns | Description |
|---|---|---|
| `bambu.fan_part()` | `integer` 0–100 | Part cooling fan % |
| `bambu.fan_aux()` | `integer` 0–100 | Auxiliary fan % |
| `bambu.fan_chamber()` | `integer` 0–100 | Chamber fan % |

#### AMS

| Function | Returns | Description |
|---|---|---|
| `bambu.ams_color(tray)` | `integer` | RGB565 color of tray 0–15. Returns `0` when absent — check `> 0` before using. |
| `bambu.ams_type(tray)` | `string` | Filament type (e.g. `"PLA"`). `""` when absent. |
| `bambu.ams_active()` | `integer` | Active tray index 0–15, or `-1` if none. |

#### Device

| Function | Returns | Description |
|---|---|---|
| `bambu.printer_name()` | `string` | User-configured printer name |

---

### Example App

```lua
-- @name Temp Watch
-- @color 0xFBE0
-- @sdk_min 2
-- @version 1.0
-- @author Example
-- @category monitor
-- @description Nozzle and bed temperatures with color-coded arcs.

local scr     = ui.screen()
local CLR_HOT = 0xFBE0   -- orange
local CLR_BED = 0x34DF   -- blue
local CLR_OK  = 0x07E0   -- green
local CLR_DIM = 0x18E3   -- track

local noz_arc = ui.arc(scr, {cx=120, cy=120, size=100, value=0,
    color=CLR_HOT, track=CLR_DIM, thickness=10})
local bed_arc = ui.arc(scr, {cx=120, cy=120, size=80, value=0,
    color=CLR_BED, track=CLR_DIM, thickness=8})
local noz_lbl = ui.label(scr, "N --°", {align="center", y=-10, font=20, color=CLR_HOT})
local bed_lbl = ui.label(scr, "B --°", {align="center", y=16,  font=16, color=CLR_BED})

sys.on_tick(function(_dt)
    local noz, noz_t = bambu.nozzle_temp(), bambu.nozzle_target()
    local bed, bed_t = bambu.bed_temp(),    bambu.bed_target()

    local function pct(cur, tgt)
        if tgt <= 0 then return 0 end
        return math.min(math.floor(cur / tgt * 100), 100)
    end

    ui.arc_set(noz_arc, pct(noz, noz_t))
    ui.arc_set(bed_arc, pct(bed, bed_t))
    ui.arc_color(noz_arc, (noz_t > 0 and math.abs(noz - noz_t) < 5) and CLR_OK or CLR_HOT)
    ui.label_set(noz_lbl, string.format("N %.0f°", noz))
    ui.label_set(bed_lbl, string.format("B %.0f°", bed))

    if bambu.state() == "FINISH" then sys.exit() end
end)
```

---

## Desktop Simulator

A macOS/Linux simulator lets you run and develop Lua apps without flashing hardware.

```bash
cd sdk/simulator
make          # builds the simulator binary
./bin/simulator ../../store/apps/mission-control.lua
```

The simulator provides mock `bambu.*` values editable in `sdk/simulator/sdk_impl/sdk_bambu.c`. Change `l_state`, `l_print_stage`, temperatures, etc. to simulate different printer states, then rebuild and run.

---

## Multi-Printer Monitoring

BambuHelper supports up to 2 simultaneous MQTT connections.

### Rotation Modes

| Mode | Behavior |
|---|---|
| **Smart** (default) | Shows the printing printer. Cycles between both when both are printing. Shows last active when neither is. |
| **Auto-rotate** | Cycles at a configurable interval (10 s – 10 min). |
| **Off** | Manual switch via physical button only. |

### MQTT Reconnect Backoff

| Phase | Attempts | Interval |
|---|---|---|
| Normal | First 5 | Every 10 s |
| Phase 2 | Next 10 | Every 60 s |
| Phase 3 | Beyond 15 | Every 120 s |

---

## Project Structure

```
include/
  config.h              Pin definitions, colors, constants (FW_VERSION, buffer sizes)
  bambu_state.h         Data structures (BambuState, PrinterConfig, ConnMode)
src/
  main.cpp              Setup/loop orchestrator
  settings.cpp          NVS persistence (WiFi, network, printer, display, power, cloud token)
  wifi_manager.cpp      WiFi STA + AP fallback, static IP
  web_server.cpp        Config portal (HTML embedded, app store API, token management)
  bambu_mqtt.cpp        MQTT over TLS, delta merge, stale recovery, offline reset
  bambu_cloud.cpp       Bambu Cloud helpers (region URLs, JWT userId extraction)
  button.cpp            Physical button / touch sensor input
  display_ui.cpp        Screen state machine, launcher grid
  display_gauges.cpp    Arc gauges, progress bar, temperature gauges
  display_anim.cpp      Boot/connect animations
  clock_mode.cpp        Digital clock display
  sdk_api.cpp           Lua SDK implementation (ui.*, sys.*, bambu.*)
  lua_runtime.cpp       Lua VM lifecycle, app loading from LittleFS
  icons.h               Pixel-art icons
store/
  apps/                 Lua app scripts
    mission-control.lua Animated state-machine printer monitor
    print-report.lua    Last print summary
    layer-timeline.lua  Per-layer speed bar chart
    crosshair.lua       Homing animation
  index.json            App store catalog
sdk/
  simulator/            Desktop simulator for Lua app development
tools/
  get_token.py          Python helper to extract Bambu Cloud token
  build_index.py        Rebuild store/index.json from app header comments
```

---

## Requirements

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- **LAN mode:** Printer with LAN mode enabled; ESP32 on same local network
- **Cloud mode:** Bambu Lab account; ESP32 with internet access

---

## Troubleshooting

### WiFi won't connect / drops frequently

SPI display cables near the ESP32 antenna cause RF interference. Route cables away from the antenna end of the board. Even 1–2 cm separation helps significantly.

Symptoms: falls back to AP mode quickly, or works only when display is disconnected.

If issues persist, perform an antenna mod by soldering two goldpins to the antenna pads.

### Printer shows "Connecting" but never connects

- **LAN Direct:** Printer and ESP32 must be on the same network. LAN mode must be enabled. Access code must be correct.
- **Bambu Cloud:** Token may have expired (~3 months). Re-extract and paste. Verify region matches your account.
- Offline printers trigger backoff — reconnect is automatic when the printer comes back.

### Display shows wrong printer

Check rotation mode in the web interface. Smart mode only auto-switches when a printer is actively printing. Use the physical button to switch manually.

### Mission Control shows "RUNNING" when printer is off

After the MQTT connection drops, state resets to IDLE immediately. If it persists after reconnect, a recovery pushall is sent automatically and the state clears within 30 s.

---

## License

MIT

---

## Credits

Based on [BambuHelper](https://github.com/Keralots/BambuHelper) by [Keralots](https://github.com/Keralots).

Major additions in this fork:
- Round GC9A01A display support (LVGL 8 rendering pipeline)
- Lua scripting SDK (`ui.*`, `sys.*`, `bambu.*`)
- App store with WiFi install/delete
- 3×2 launcher grid with live clock tile
- LittleFS app storage
- Desktop simulator for Lua development
- Mission Control unified animated status app (9 state animations)
- NTP clock with timezone support
- v2.5 MQTT reliability fixes
