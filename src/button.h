#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

// CST816S I2C touch controller driver for Waveshare ESP32-S3-Touch-LCD-1.28.
// Call initButton() once after Wire is available.

void initButton();

// True once on release of a short touch (held < 800 ms).
bool wasButtonPressed();

// True once when a touch has been held for >= 800 ms. Fires once per hold.
bool wasButtonLongPressed();

// Returns true and fills *x/*y if a finger is currently on the screen.
bool getTouchXY(int16_t* x, int16_t* y);

#endif // BUTTON_H
