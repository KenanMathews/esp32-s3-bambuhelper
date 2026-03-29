#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <TFT_eSPI.h>

enum ScreenState {
  SCREEN_SPLASH,
  SCREEN_AP_MODE,
  SCREEN_CONNECTING_WIFI,
  SCREEN_WIFI_CONNECTED,
  SCREEN_CONNECTING_MQTT,
  SCREEN_IDLE,
  SCREEN_PRINTING,
  SCREEN_FINISHED,
  SCREEN_CLOCK,
  SCREEN_OFF,
  SCREEN_LAUNCHER,      // long-press overlay menu
  SCREEN_PRINTER_LIST,  // printer-selection screen
  SCREEN_STORE,         // app store placeholder
  SCREEN_APP            // running Lua app
};

// TFT_eSPI instance — used by lvgl_port.cpp for SPI flush.
// No other file should call TFT_eSPI rendering functions directly.
extern TFT_eSPI tft;

void initDisplay();
void updateDisplay();
void setScreenState(ScreenState state);
ScreenState getScreenState();
void setBacklight(uint8_t level);
void applyDisplaySettings();     // re-apply rotation / bg color after settings change
void triggerDisplayTransition(); // called on multi-printer rotation to reset gauges

#endif // DISPLAY_UI_H
