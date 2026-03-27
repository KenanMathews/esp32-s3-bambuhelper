#include "button.h"
#include <Wire.h>

// ---------------------------------------------------------------------------
//  CST816S I2C touch controller — Waveshare ESP32-S3-Touch-LCD-1.28
// ---------------------------------------------------------------------------
#ifndef TOUCH_SDA
#define TOUCH_SDA  6
#endif
#ifndef TOUCH_SCL
#define TOUCH_SCL  7
#endif
#ifndef TOUCH_RST
#define TOUCH_RST  13
#endif
#ifndef TOUCH_INT
#define TOUCH_INT  -1
#endif
#define CST816S_ADDR  0x15

#define CST816S_REG_FINGERS  0x02
#define CST816S_REG_XH       0x03
#define CST816S_REG_XL       0x04
#define CST816S_REG_YH       0x05
#define CST816S_REG_YL       0x06

// ---------------------------------------------------------------------------
//  State
// ---------------------------------------------------------------------------
static bool          stableState   = false;
static bool          lastRaw       = false;
static unsigned long lastChangeMs  = 0;
static unsigned long pressStartMs  = 0;
static bool          longFired     = false;
static bool          shortConsumed = false;

static const unsigned long DEBOUNCE_MS   = 50;
static const unsigned long LONG_PRESS_MS = 800;

// ---------------------------------------------------------------------------
//  CST816S helpers
// ---------------------------------------------------------------------------
static int cst816s_readReg(uint8_t reg) {
  Wire.beginTransmission(CST816S_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  Wire.requestFrom((uint8_t)CST816S_ADDR, (uint8_t)1);
  if (!Wire.available()) return -1;
  return Wire.read();
}

static bool cst816s_touched() {
  // Do not gate on INT — CST816S INT only pulses briefly on touch-down,
  // it does not stay low while the finger is held. Poll registers directly.
  int fingers = cst816s_readReg(CST816S_REG_FINGERS);
  return fingers > 0;
}

// ---------------------------------------------------------------------------
//  initButton
// ---------------------------------------------------------------------------
void initButton() {
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(5);
  digitalWrite(TOUCH_RST, HIGH);
  delay(50);
  Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000);

  stableState   = false;
  lastRaw       = false;
  lastChangeMs  = 0;
  pressStartMs  = 0;
  longFired     = false;
  shortConsumed = false;
}

// ---------------------------------------------------------------------------
//  Internal: debounced raw state
// ---------------------------------------------------------------------------
static bool readDebounced() {
  bool raw = cst816s_touched();

  if (raw != lastRaw) {
    lastChangeMs = millis();
    lastRaw = raw;
  }
  if ((millis() - lastChangeMs) < DEBOUNCE_MS) return stableState;
  return raw;
}

// ---------------------------------------------------------------------------
//  wasButtonPressed — true once on release of a short press (< 800 ms)
// ---------------------------------------------------------------------------
bool wasButtonPressed() {
  bool raw = readDebounced();
  bool result = false;

  if (raw && !stableState) {
    pressStartMs  = millis();
    longFired     = false;
    shortConsumed = false;
  } else if (!raw && stableState) {
    if (!longFired && !shortConsumed && pressStartMs > 0) result = true;
    pressStartMs  = 0;
    longFired     = false;
    shortConsumed = false;
  }

  stableState = raw;
  return result;
}

// ---------------------------------------------------------------------------
//  wasButtonLongPressed — true once when held >= 800 ms
// ---------------------------------------------------------------------------
bool wasButtonLongPressed() {
  bool raw = readDebounced();
  if (!raw) return false;
  if (pressStartMs == 0) pressStartMs = millis();
  if (longFired) return false;
  if ((millis() - pressStartMs) < LONG_PRESS_MS) return false;

  longFired     = true;
  shortConsumed = true;
  return true;
}

// ---------------------------------------------------------------------------
//  getTouchXY — returns current touch coordinates from CST816S
// ---------------------------------------------------------------------------
bool getTouchXY(int16_t* x, int16_t* y) {
  int fingers = cst816s_readReg(CST816S_REG_FINGERS);
  if (fingers <= 0) return false;

  int xh = cst816s_readReg(CST816S_REG_XH);
  int xl = cst816s_readReg(CST816S_REG_XL);
  int yh = cst816s_readReg(CST816S_REG_YH);
  int yl = cst816s_readReg(CST816S_REG_YL);

  if (xh < 0 || xl < 0 || yh < 0 || yl < 0) return false;

  *x = (int16_t)(((xh & 0x0F) << 8) | xl);
  *y = (int16_t)(((yh & 0x0F) << 8) | yl);
  return true;
}
