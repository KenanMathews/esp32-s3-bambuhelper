#ifndef SCREEN_PRINTER_H
#define SCREEN_PRINTER_H

#include <lvgl.h>
#include "bambu_state.h"
#include "display_ui.h"   // ScreenState

// Call once at boot — allocates the static LVGL screen and all widgets.
void printerScreenInit();

// Call every update tick — pushes latest BambuState into widgets.
// state must be SCREEN_PRINTING, SCREEN_IDLE, or SCREEN_FINISHED.
void printerScreenUpdate(const PrinterSlot& slot, ScreenState state);

// Returns the persistent LVGL screen object for lv_scr_load().
lv_obj_t* printerScreenGet();

// Reset smooth-gauge interpolation state (call on printer rotation / settings change).
void printerScreenTransition();

#endif // SCREEN_PRINTER_H
