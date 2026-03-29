#ifndef DISPLAY_LAUNCHER_H
#define DISPLAY_LAUNCHER_H
#include <stdint.h>

#define LAUNCHER_GRID_COLS  3
#define LAUNCHER_GRID_ROWS  2
#define LAUNCHER_GRID_SIZE  6

enum LauncherItem : uint8_t {
    LAUNCHER_DASHBOARD  = 0,
    LAUNCHER_CLOCK      = 1,
    LAUNCHER_STORE      = 2,
    LAUNCHER_SETTINGS   = 3,
    LAUNCHER_USER_BASE  = 4,
    LAUNCHER_ITEM_COUNT = LAUNCHER_GRID_SIZE
};

void   launcherEnter();
int8_t launcherUpdate();

#endif // DISPLAY_LAUNCHER_H
