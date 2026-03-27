#ifndef LVGL_PORT_H
#define LVGL_PORT_H

// LVGL port for BambuHelper
// Display: GC9A01A 240×240 round via TFT_eSPI
// Touch:   CST816S I2C via button.h getTouchXY()

// Call once after TFT and button are initialized.
// Sets up LVGL display driver and touch input driver.
void lvglPortInit();

// Call every loop iteration when LVGL is active.
// Drives lv_timer_handler() at the required cadence.
void lvglPortTick();

#endif // LVGL_PORT_H
