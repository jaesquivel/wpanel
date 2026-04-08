#pragma once

#include "../../include/config.h"
#include <Arduino.h>
#include <XPT2046_Touchscreen.h>

// ----------------------------------------------------------------------------
// Touch event types
// ----------------------------------------------------------------------------
enum class TouchEvent {
  NONE,
  PRESS,  // Initial contact
  HOLD,   // Finger held down (fires periodically)
  RELEASE // Finger lifted
};

struct TouchPoint {
  int16_t x = 0; // Calibrated screen X (0 to SCREEN_WIDTH-1)
  int16_t y = 0; // Calibrated screen Y (0 to SCREEN_HEIGHT-1)
  uint16_t pressure = 0;
  bool valid = false;
};

// ----------------------------------------------------------------------------
// Calibration data (can be stored/loaded from NVS in a future revision)
// ----------------------------------------------------------------------------
struct TouchCalibration {
  float xScale; // Multiplier: screen_x = (raw_x - xMin) * xScale
  float yScale;
  int16_t xMin;
  int16_t yMin;
  bool swapXY; // Swap axes when display is in landscape
  bool invertX;
  bool invertY;
};

// ----------------------------------------------------------------------------
// TouchManager
// ----------------------------------------------------------------------------
class TouchManager {
public:
  TouchManager();

  bool begin();

  // Call from main loop / task — updates internal state and returns event
  TouchEvent update();

  // Last known calibrated position (only valid when lastEvent != NONE)
  TouchPoint getPoint() const { return _current; }

  // Raw ADC values (for calibration utility)
  TS_Point getRaw();

  // Run an interactive calibration routine (blocks, draws to display via SPI)
  // Writes results into the active calibration struct
  void runCalibration(uint16_t screenW, uint16_t screenH);

  // Simple hit-test helper
  static bool hitTest(const TouchPoint &pt, int16_t rx, int16_t ry, int16_t rw,
                      int16_t rh);

  // Hold detection threshold
  uint32_t holdThresholdMs = 600;

private:
  XPT2046_Touchscreen _ts;
  TouchCalibration _cal;
  TouchPoint _current;
  TouchEvent _lastEvent;

  bool _wasTouched = false;
  uint32_t _pressStartMs = 0;
  uint32_t _lastDebounce = 0;
  bool _holdFired = false;

  TouchPoint _mapRaw(TS_Point raw) const;
  void _loadDefaultCalibration();
};
