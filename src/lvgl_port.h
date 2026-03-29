#ifndef LVGL_PORT_H
#define LVGL_PORT_H

// LVGL port for BambuHelper
// Board:   Waveshare ESP32-S3-Touch-LCD-1.28
// MCU:     ESP32-S3 (dual-core Xtensa LX7, 240MHz, 16MB Flash, 2MB PSRAM)
// Display: GC9A01A 240×240 round TFT, SPI2/FSPI @ 40MHz
//          MOSI=11  SCLK=10  CS=9  DC=8  RST=14  BL=2
// Touch:   CST816S capacitive, I2C @ 400kHz, addr 0x15
//          SDA=6  SCL=7  INT=5  RST=13

// Call once after TFT and button are initialized.
// Sets up LVGL display driver and touch input driver.
void lvglPortInit();

// Call every loop iteration when LVGL is active.
// Drives lv_timer_handler() at the required cadence.
void lvglPortTick();

#endif // LVGL_PORT_H
