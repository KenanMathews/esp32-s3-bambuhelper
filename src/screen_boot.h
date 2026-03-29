#ifndef SCREEN_BOOT_H
#define SCREEN_BOOT_H

#include <lvgl.h>
#include "display_ui.h"  // ScreenState

// Call once after lvglPortInit() — creates all persistent boot/connection screens.
void bootScreensInit();

// Returns the persistent LVGL screen for the given state.
// Valid: SCREEN_SPLASH, SCREEN_AP_MODE, SCREEN_CONNECTING_WIFI,
//        SCREEN_WIFI_CONNECTED, SCREEN_CONNECTING_MQTT, SCREEN_OFF
lv_obj_t* bootScreenGet(ScreenState state);

// Call when entering a screen to refresh one-time content (e.g. IP address).
void bootScreenPrepare(ScreenState state);

// Call each display tick (~250 ms) to refresh dynamic labels on connecting screens.
void bootScreenUpdate(ScreenState state);

#endif // SCREEN_BOOT_H
