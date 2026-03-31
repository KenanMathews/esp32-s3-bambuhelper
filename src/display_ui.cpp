/*
 * display_ui.cpp — LVGL screen state machine
 *
 * Board:  Waveshare ESP32-S3-Touch-LCD-1.28
 *         GC9A01A 240×240 round, SPI2 @ 40MHz | CST816S touch, I2C 0x15
 *
 * Owns the TFT_eSPI instance (used only by lvgl_port.cpp for SPI flushing).
 * All visible rendering is done through LVGL screens managed by:
 *   screen_boot.cpp     — splash, AP mode, connecting, WiFi connected, off
 *   screen_printer.cpp  — idle, printing, finished
 *   screen_clock_text.cpp — digital clock (text mode)
 *   display_launcher.cpp  — long-press menu (already LVGL)
 *
 * The pong clock (clock_pong.cpp) still uses TFT_eSPI directly.
 * While it is active, lvglPortTick() must NOT be called (main.cpp handles this).
 * LVGL re-renders the next screen from scratch when lv_scr_load() is called,
 * so TFT_eSPI pong content is naturally overwritten on exit.
 */

#include "display_ui.h"
#include "screen_boot.h"
#include "screen_printer.h"
#include "screen_clock_text.h"
#include "screen_store.h"
#include "screen_app.h"
#include "screen_info.h"
#include "display_launcher.h"
#include "lvgl_port.h"
#include "config.h"
#include "settings.h"
#include "bambu_state.h"
#include "bambu_mqtt.h"
#include "clock_pong.h"
#include <lvgl.h>
#include <time.h>

// TFT_eSPI instance — used only by lvgl_port.cpp disp_flush callback.
TFT_eSPI tft = TFT_eSPI();

static ScreenState currentScreen = SCREEN_SPLASH;
static ScreenState prevScreen    = SCREEN_SPLASH;
static unsigned long lastDisplayUpdate = 0;

// ---------------------------------------------------------------------------
//  Backlight
// ---------------------------------------------------------------------------
void setBacklight(uint8_t level) {
#if defined(BACKLIGHT_PIN) && BACKLIGHT_PIN >= 0
    analogWrite(BACKLIGHT_PIN, level);
#endif
}

// ---------------------------------------------------------------------------
//  Map ScreenState to its LVGL screen object
// ---------------------------------------------------------------------------
static lv_obj_t* screenObjFor(ScreenState state) {
    switch (state) {
        case SCREEN_SPLASH:
        case SCREEN_AP_MODE:
        case SCREEN_CONNECTING_WIFI:
        case SCREEN_WIFI_CONNECTED:
        case SCREEN_CONNECTING_MQTT:
        case SCREEN_OFF:
            return bootScreenGet(state);
        case SCREEN_IDLE:
        case SCREEN_PRINTING:
        case SCREEN_FINISHED:
            return printerScreenGet();
        case SCREEN_CLOCK:
            return clockTextScreenGet();
        case SCREEN_STORE:
            return storeScreenGet();
        case SCREEN_APP:
            return appScreenGet();
        case SCREEN_INFO:
            return infoScreenGet();
        case SCREEN_LAUNCHER:
            return nullptr;  // launcher manages its own screen
        default:
            return nullptr;
    }
}

// ---------------------------------------------------------------------------
//  initDisplay — called once from setup()
//  Initializes TFT + LVGL + all persistent screens, then shows splash.
// ---------------------------------------------------------------------------
void initDisplay() {
    Serial.println("Display: init TFT...");
    tft.init();
    tft.setRotation(dispSettings.rotation);
    tft.fillScreen(0x0000);  // black while LVGL starts
    Serial.println("Display: TFT ready");

#if defined(BACKLIGHT_PIN) && BACKLIGHT_PIN >= 0
    pinMode(BACKLIGHT_PIN, OUTPUT);
    setBacklight(0);  // keep off while building screens
#endif

    // Initialize LVGL and register display + touch drivers
    lvglPortInit();

    // Build all persistent LVGL screens
    bootScreensInit();
    printerScreenInit();
    clockTextScreenInit();
    storeScreenInit();
    appScreenInit();
    infoScreenInit();

    // Show splash immediately
    lv_scr_load(bootScreenGet(SCREEN_SPLASH));
    lvglPortTick();  // render one frame right away

    setBacklight(200);
    Serial.println("Display: ready");
}

// ---------------------------------------------------------------------------
//  applyDisplaySettings — called after settings change (rotation, bg color, etc.)
// ---------------------------------------------------------------------------
void applyDisplaySettings() {
    tft.setRotation(dispSettings.rotation);
    // Force LVGL to re-render the current screen from scratch
    lv_obj_invalidate(lv_scr_act());
}

// ---------------------------------------------------------------------------
//  triggerDisplayTransition — called on multi-printer rotation
// ---------------------------------------------------------------------------
void triggerDisplayTransition() {
    printerScreenTransition();   // reset smooth-gauge state
    lv_obj_invalidate(lv_scr_act());
}

// ---------------------------------------------------------------------------
//  setScreenState — change the active screen
// ---------------------------------------------------------------------------
void setScreenState(ScreenState state) {
    if (state == currentScreen) return;
    currentScreen = state;

    // Backlight: restore when leaving SCREEN_OFF
    if (prevScreen == SCREEN_OFF && state != SCREEN_OFF) {
        setBacklight(brightness);
    }

    // Prepare one-time content before showing
    bootScreenPrepare(state);

    if (state == SCREEN_CLOCK) {
        if (!dispSettings.pongClock) {
            clockTextScreenReset();
        } else {
            resetPongClock();
        }
    }

    // Load LVGL screen (launcher, store, and app manage their own lv_scr_load)
    if (state == SCREEN_STORE) {
        storeScreenEnter();  // handles lv_scr_load + HTTP fetch
    } else if (state == SCREEN_INFO) {
        infoScreenEnter();   // updates live data then loads screen
    } else if (state != SCREEN_LAUNCHER && state != SCREEN_APP) {
        // For pong clock, clockTextScreenGet() returns a blank screen
        // that serves as the LVGL owner while pong draws over TFT_eSPI
        lv_obj_t* scr = screenObjFor(state);
        if (scr) lv_scr_load(scr);
    }

    // SCREEN_OFF: backlight off after screen load
    if (state == SCREEN_OFF) {
        setBacklight(0);
    }

    prevScreen = state;
}

ScreenState getScreenState() {
    return currentScreen;
}

// ---------------------------------------------------------------------------
//  checkNightMode — call once per loop to apply night/screensaver brightness
// ---------------------------------------------------------------------------
void checkNightMode() {
    // SCREEN_OFF is handled by setBacklight(0) in setScreenState — don't override
    if (currentScreen == SCREEN_OFF) return;

    uint8_t targetBrightness = brightness;

    // Night mode requires NTP sync
    if (dpSettings.nightModeEnabled) {
        struct tm now;
        if (getLocalTime(&now, 0)) {
            uint8_t h = now.tm_hour;
            bool isNight = false;
            if (dpSettings.nightStartHour < dpSettings.nightEndHour) {
                isNight = (h >= dpSettings.nightStartHour && h < dpSettings.nightEndHour);
            } else {
                // Wrap-around: e.g. 22→7
                isNight = (h >= dpSettings.nightStartHour || h < dpSettings.nightEndHour);
            }
            if (isNight) targetBrightness = dpSettings.nightBrightness;
        }
    }

    // Screensaver brightness on idle/clock screens (0 = disabled)
    if (dpSettings.screensaverBrightness > 0 &&
        (currentScreen == SCREEN_CLOCK || currentScreen == SCREEN_IDLE ||
         currentScreen == SCREEN_CONNECTING_MQTT)) {
        // Only dim further (screensaver ≤ night ≤ brightness)
        if (dpSettings.screensaverBrightness < targetBrightness)
            targetBrightness = dpSettings.screensaverBrightness;
    }

    setBacklight(targetBrightness);
}

// ---------------------------------------------------------------------------
//  updateDisplay — called every loop() iteration
//  Throttled to DISPLAY_UPDATE_MS except for pong which has its own cadence.
// ---------------------------------------------------------------------------
void updateDisplay() {
    // Pong clock: runs at ~50fps in main.cpp (tickPongClock), skip LVGL updates
    if (currentScreen == SCREEN_CLOCK && dispSettings.pongClock) {
        tickPongClock();
        return;
    }

    unsigned long now = millis();
    // Adaptive refresh: ~12 FPS while gauges are animating, 4 FPS at idle
    unsigned long updateInterval = (currentScreen == SCREEN_PRINTING && !printerGaugesSettled())
                                   ? 83UL : DISPLAY_UPDATE_MS;
    if (now - lastDisplayUpdate < updateInterval) return;
    lastDisplayUpdate = now;

    // Update dynamic content for the current screen
    switch (currentScreen) {
        case SCREEN_CONNECTING_WIFI:
        case SCREEN_CONNECTING_MQTT:
            bootScreenUpdate(currentScreen);
            break;

        case SCREEN_WIFI_CONNECTED:
            bootScreenUpdate(currentScreen);
            break;

        case SCREEN_IDLE:
        case SCREEN_PRINTING:
        case SCREEN_FINISHED:
            printerScreenUpdate(displayedPrinter(), currentScreen);
            break;

        case SCREEN_CLOCK:
            clockTextScreenUpdate();
            break;

        case SCREEN_APP:
            appScreenUpdate();
            break;

        case SCREEN_STORE:
            storeScreenUpdate();
            break;

        case SCREEN_INFO:
            infoScreenUpdate();
            break;

        default:
            break;
    }
}
