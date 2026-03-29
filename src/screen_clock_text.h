#ifndef SCREEN_CLOCK_TEXT_H
#define SCREEN_CLOCK_TEXT_H

#include <lvgl.h>

// Call once after lvglPortInit() to create the persistent text-clock screen.
void clockTextScreenInit();

// Returns the persistent LVGL screen object for lv_scr_load().
lv_obj_t* clockTextScreenGet();

// Call from the display update tick to refresh time/date labels.
// Redraws only when the minute changes.
void clockTextScreenUpdate();

// Force the next call to clockTextScreenUpdate() to redraw unconditionally.
void clockTextScreenReset();

#endif // SCREEN_CLOCK_TEXT_H
