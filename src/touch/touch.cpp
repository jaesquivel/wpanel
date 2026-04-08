#include "touch.h"

// ============================================================================
// TouchManager implementation
// ============================================================================

// Dedicated SPI bus for XPT2046 (CLK=25, MISO=39, MOSI=32)
// The TFT uses HSPI (CLK=14, MISO=12, MOSI=13) — these must not share a bus.
static SPIClass _touchSPI(VSPI);

TouchManager::TouchManager()
    : _ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN)
    , _lastEvent(TouchEvent::NONE)
{
    _loadDefaultCalibration();
}

bool TouchManager::begin()
{
    _touchSPI.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
    _ts.begin(_touchSPI);
    _ts.setRotation(SCREEN_ROTATION);

    Serial.println("[Touch] XPT2046 init OK");
    Serial.printf("[Touch] Cal: xMin=%d xMax=%d yMin=%d yMax=%d\n",
                  TOUCH_X_MIN, TOUCH_X_MAX, TOUCH_Y_MIN, TOUCH_Y_MAX);
    return true;
}

// ----------------------------------------------------------------------------
// Default calibration derived from config constants
// ----------------------------------------------------------------------------
void TouchManager::_loadDefaultCalibration()
{
    _cal.xMin   = TOUCH_X_MIN;
    _cal.yMin   = TOUCH_Y_MIN;
    _cal.xScale = (float)SCREEN_WIDTH  / (TOUCH_X_MAX - TOUCH_X_MIN);
    _cal.yScale = (float)SCREEN_HEIGHT / (TOUCH_Y_MAX - TOUCH_Y_MIN);
    _cal.swapXY  = false;
    _cal.invertX = false;
    _cal.invertY = false;
}

// ----------------------------------------------------------------------------
// Map raw XPT2046 ADC values to screen coordinates
// ----------------------------------------------------------------------------
TouchPoint TouchManager::_mapRaw(TS_Point raw) const
{
    TouchPoint pt;
    pt.pressure = (uint16_t)raw.z;

    int16_t rx = raw.x;
    int16_t ry = raw.y;

    if (_cal.swapXY) {
        int16_t tmp = rx; rx = ry; ry = tmp;
    }

    float sx = (rx - _cal.xMin) * _cal.xScale;
    float sy = (ry - _cal.yMin) * _cal.yScale;

    if (_cal.invertX) sx = SCREEN_WIDTH  - 1 - sx;
    if (_cal.invertY) sy = SCREEN_HEIGHT - 1 - sy;

    pt.x = (int16_t)constrain((int)sx, 0, SCREEN_WIDTH  - 1);
    pt.y = (int16_t)constrain((int)sy, 0, SCREEN_HEIGHT - 1);
    pt.valid = true;
    return pt;
}

// ----------------------------------------------------------------------------
// Main update — call once per loop iteration
// ----------------------------------------------------------------------------
TouchEvent TouchManager::update()
{
    uint32_t now = millis();
    TouchEvent evt = TouchEvent::NONE;

    // IRQ pin goes LOW when touched
    bool touched = _ts.tirqTouched() && _ts.touched();

    if (touched) {
        TS_Point raw = _ts.getPoint();

        // Filter out very light or noise touches
        if (raw.z < TOUCH_PRESSURE_MIN) {
            // Pressure too low — treat as not touched
            touched = false;
        } else {
            _current = _mapRaw(raw);
        }
    }

    if (touched && !_wasTouched) {
        // --- New press ---
        if ((now - _lastDebounce) >= TOUCH_DEBOUNCE_MS) {
            _pressStartMs = now;
            _holdFired    = false;
            _wasTouched   = true;
            _lastDebounce = now;
            evt = TouchEvent::PRESS;
        }

    } else if (touched && _wasTouched) {
        // --- Held down ---
        if (!_holdFired && (now - _pressStartMs) >= holdThresholdMs) {
            _holdFired = true;
            evt = TouchEvent::HOLD;
        }

    } else if (!touched && _wasTouched) {
        // --- Released ---
        _wasTouched = false;
        _current.valid = false;
        evt = TouchEvent::RELEASE;
    }

    _lastEvent = evt;
    return evt;
}

// ----------------------------------------------------------------------------
// Raw value access (for calibration)
// ----------------------------------------------------------------------------
TS_Point TouchManager::getRaw()
{
    return _ts.getPoint();
}

// ----------------------------------------------------------------------------
// Interactive calibration (blocking)
// Samples 3 known points, computes new calibration params.
// In production you'd draw crosshairs on screen; here we output to Serial.
// ----------------------------------------------------------------------------
void TouchManager::runCalibration(uint16_t screenW, uint16_t screenH)
{
    Serial.println("[Touch] === Calibration Mode ===");
    Serial.println("[Touch] Touch top-left corner when prompted...");

    // We'll collect raw samples at 3 target positions
    // Target positions: top-left, top-right, bottom-centre
    const uint8_t  SAMPLES  = 16;
    const uint32_t TIMEOUT  = 10000;  // 10s per point

    struct CalPoint { uint16_t tx, ty; int32_t rx, ry; };
    CalPoint pts[3] = {
        { 20,          20,           0, 0 },
        { (uint16_t)(screenW - 20), 20,           0, 0 },
        { (uint16_t)(screenW / 2),  (uint16_t)(screenH - 20), 0, 0 },
    };

    for (uint8_t i = 0; i < 3; ++i) {
        Serial.printf("[Touch] Touch point %d  (target x=%d y=%d) ...\n",
                      i + 1, pts[i].tx, pts[i].ty);

        uint32_t t0 = millis();
        bool got = false;
        while (!got && (millis() - t0 < TIMEOUT)) {
            if (_ts.tirqTouched() && _ts.touched()) {
                int32_t ax = 0, ay = 0;
                for (uint8_t s = 0; s < SAMPLES; ++s) {
                    TS_Point p = _ts.getPoint();
                    ax += p.x; ay += p.y;
                    delay(10);
                }
                pts[i].rx = ax / SAMPLES;
                pts[i].ry = ay / SAMPLES;
                Serial.printf("[Touch] Got raw: %ld, %ld\n", pts[i].rx, pts[i].ry);
                got = true;
                delay(500);  // Debounce lift
            }
            delay(20);
        }
        if (!got) {
            Serial.println("[Touch] Timeout — calibration aborted");
            return;
        }
    }

    // Compute linear mapping from 2 horizontal + 1 vertical sample
    // xScale = (screen_x1 - screen_x0) / (raw_x1 - raw_x0)
    float newXScale = (float)(pts[1].tx - pts[0].tx) / (float)(pts[1].rx - pts[0].rx);
    float newYScale = (float)(pts[2].ty - pts[0].ty) / (float)(pts[2].ry - pts[0].ry);
    int16_t newXMin = (int16_t)(pts[0].rx - pts[0].tx / newXScale);
    int16_t newYMin = (int16_t)(pts[0].ry - pts[0].ty / newYScale);

    _cal.xScale = newXScale;
    _cal.yScale = newYScale;
    _cal.xMin   = newXMin;
    _cal.yMin   = newYMin;

    Serial.printf("[Touch] New cal: xMin=%d  xScale=%.4f  yMin=%d  yScale=%.4f\n",
                  newXMin, newXScale, newYMin, newYScale);
    Serial.println("[Touch] Save these values to config.h TOUCH_X_MIN/MAX etc.");
}

// ----------------------------------------------------------------------------
// Hit test utility
// ----------------------------------------------------------------------------
bool TouchManager::hitTest(const TouchPoint& pt,
                            int16_t rx, int16_t ry, int16_t rw, int16_t rh)
{
    if (!pt.valid) return false;
    return (pt.x >= rx && pt.x < (rx + rw) &&
            pt.y >= ry && pt.y < (ry + rh));
}
