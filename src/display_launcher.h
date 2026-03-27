#ifndef DISPLAY_LAUNCHER_H
#define DISPLAY_LAUNCHER_H

#include <Arduino.h>

// Launcher menu items
enum LauncherItem : uint8_t {
  LAUNCHER_DASHBOARD = 0,
  LAUNCHER_CLOCK     = 1,
  LAUNCHER_SETTINGS  = 2,
  LAUNCHER_PRINTER   = 3,
  LAUNCHER_ITEM_COUNT = 4
};

// Call once when entering SCREEN_LAUNCHER to reset state and draw the menu.
void launcherEnter();

// Call every loop while in SCREEN_LAUNCHER.
// Returns the selected LauncherItem index (0-3) when the user taps an item,
// or -1 while waiting, or -2 on auto-dismiss (5 s timeout / long-press again).
int8_t launcherUpdate();

#endif // DISPLAY_LAUNCHER_H
