#include "ui.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// UIManager implementation
// ============================================================================

UIManager::UIManager(DisplayManager& display, TouchManager& touch, CANBusManager& can)
    : _display(display), _touch(touch), _can(can)
{}

void UIManager::begin()
{
    _currentScreen = ScreenId::HOME;
    _dirty         = true;
    Serial.println("[UI] UIManager started, screen=HOME");
}

// ----------------------------------------------------------------------------
// update() — main UI tick
// ----------------------------------------------------------------------------
void UIManager::update()
{
    uint32_t now = millis();

    // Process touch input
    TouchEvent evt = _touch.update();
    if (evt != TouchEvent::NONE) {
        TouchPoint pt = _touch.getPoint();

        // Let nav bar intercept first
        if (!_handleNavBarTouch(pt, evt)) {
            // Route to active screen
            switch (_currentScreen) {
                case ScreenId::HOME:     _handleTouchHome    (pt, evt); break;
                case ScreenId::HVAC:     _handleTouchHvac    (pt, evt); break;
                case ScreenId::LIGHTS:   _handleTouchLights  (pt, evt); break;
                case ScreenId::SETTINGS: _handleTouchSettings(pt, evt); break;
                default: break;
            }
        }
    }

    // Periodic sensor redraw
    if ((now - _lastSensorMs) >= SENSOR_POLL_INTERVAL_MS) {
        _lastSensorMs = now;
        _dirty = true;
    }

    // Only redraw when dirty and enough time has passed
    if (_dirty && (now - _lastDrawMs) >= UI_UPDATE_INTERVAL_MS) {
        _lastDrawMs = now;
        _dirty      = false;
        _drawFrame();
    }
}

// ----------------------------------------------------------------------------
// Navigate to screen
// ----------------------------------------------------------------------------
void UIManager::navigateTo(ScreenId screen)
{
    if (screen == _currentScreen) return;
    _currentScreen    = screen;
    _pressedNavBtn    = 0xFF;
    _pressedLightZone = 0xFF;
    _dirty            = true;
    Serial.printf("[UI] Navigate -> screen %d\n", (int)screen);
}

// ----------------------------------------------------------------------------
// Full frame draw: status bar + active screen + nav bar
// ----------------------------------------------------------------------------
void UIManager::_drawFrame()
{
    _display.fillScreen(COLOR_BACKGROUND);

    // Status bar — time placeholder (RTC not implemented here)
    bool canOk = _can.isRunning() &&
                 ((millis() - _can.lastRxMs()) < CAN_TIMEOUT_MS || _can.rxCount() == 0);
    _display.drawStatusBar("--:--", canOk);

    // Active screen content
    switch (_currentScreen) {
        case ScreenId::HOME:     _drawHomeScreen();     break;
        case ScreenId::HVAC:     _drawHvacScreen();     break;
        case ScreenId::LIGHTS:   _drawLightsScreen();   break;
        case ScreenId::SETTINGS: _drawSettingsScreen(); break;
        default: break;
    }

    _drawNavBar();
    _display.pushBuffer();
}

// ----------------------------------------------------------------------------
// Navigation bar
// ----------------------------------------------------------------------------
void UIManager::_drawNavBar()
{
    TFT_eSprite& spr = _display.getSprite();

    spr.fillRect(0, NAV_BAR_Y, SCREEN_WIDTH, NAV_BAR_H, COLOR_PANEL_BG);
    spr.drawFastHLine(0, NAV_BAR_Y, SCREEN_WIDTH, COLOR_DIVIDER);

    const char* labels[] = { "Home", "HVAC", "Lights", "Settings" };
    for (uint8_t i = 0; i < (uint8_t)ScreenId::COUNT; ++i) {
        int16_t bx = i * NAV_BTN_W;
        bool    active = ((ScreenId)i == _currentScreen);

        uint16_t txtColor = active ? COLOR_ACCENT : COLOR_TEXT_SECONDARY;
        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(txtColor, COLOR_PANEL_BG);
        spr.setTextFont(FONT_SMALL);
        spr.setTextSize(1);
        spr.drawString(labels[i], bx + NAV_BTN_W / 2, NAV_BAR_Y + NAV_BAR_H / 2);

        // Active underline
        if (active) {
            spr.fillRect(bx + 4, NAV_BAR_Y + NAV_BAR_H - 3, NAV_BTN_W - 8, 3, COLOR_ACCENT);
        }
    }
}

bool UIManager::_handleNavBarTouch(const TouchPoint& pt, TouchEvent evt)
{
    if (!TouchManager::hitTest(pt, 0, NAV_BAR_Y, SCREEN_WIDTH, NAV_BAR_H)) return false;

    uint8_t btnIdx = (uint8_t)(pt.x / NAV_BTN_W);
    if (btnIdx >= (uint8_t)ScreenId::COUNT) return true;

    if (evt == TouchEvent::PRESS) {
        _pressedNavBtn = btnIdx;
    } else if (evt == TouchEvent::RELEASE) {
        if (_pressedNavBtn == btnIdx) {
            navigateTo((ScreenId)btnIdx);
        }
        _pressedNavBtn = 0xFF;
    }
    return true;
}

// ============================================================================
// HOME SCREEN
// ============================================================================

void UIManager::_drawHomeScreen()
{
    TFT_eSprite& spr = _display.getSprite();
    const SystemState& s = _can.state();

    // Determine aggregate light state (ON if any zone is on)
    bool anyOn = false;
    for (uint8_t z = 0; z < SystemState::MAX_ZONES; ++z) {
        if (s.lightValid[z] && s.lightState[z]) { anyOn = true; break; }
    }

    // Touch coordinates — top-left debug overlay
    TouchPoint tp = _touch.getPoint();
    char tBuf[24];
    if (tp.valid) {
        snprintf(tBuf, sizeof(tBuf), "X:%-4d Y:%-4d P:%-4d", tp.x, tp.y, tp.pressure);
    } else {
        strncpy(tBuf, "X:---- Y:---- P:----", sizeof(tBuf));
    }
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.setTextFont(1);
    spr.setTextSize(1);
    spr.drawString(tBuf, 4, UI_CONTENT_Y + 2);

    // Single centred ON/OFF button
    // Content area: y=UI_CONTENT_Y(22) to y=NAV_BAR_Y(210) = 188px tall
    static constexpr int16_t BTN_W = 160;
    static constexpr int16_t BTN_H =  60;
    int16_t bx = (SCREEN_WIDTH  - BTN_W) / 2;   // 80
    int16_t by = UI_CONTENT_Y + (188 - BTN_H) / 2;  // centred in content area

    spr.drawRoundRect(bx, by, BTN_W, BTN_H, 10, TFT_WHITE);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.setTextFont(FONT_MEDIUM);
    spr.setTextSize(1);
    spr.drawString(anyOn ? "ON" : "OFF", bx + BTN_W / 2, by + BTN_H / 2);
}

void UIManager::_handleTouchHome(const TouchPoint& pt, TouchEvent evt)
{
    if (evt != TouchEvent::RELEASE) return;

    static constexpr int16_t BTN_W = 160;
    static constexpr int16_t BTN_H =  60;
    int16_t bx = (SCREEN_WIDTH  - BTN_W) / 2;
    int16_t by = UI_CONTENT_Y + (188 - BTN_H) / 2;

    if (!TouchManager::hitTest(pt, bx, by, BTN_W, BTN_H)) return;

    // Toggle: if any zone is on → all off, else → all on
    bool anyOn = false;
    for (uint8_t z = 0; z < SystemState::MAX_ZONES; ++z) {
        if (_can.state().lightValid[z] && _can.state().lightState[z]) {
            anyOn = true; break;
        }
    }
    uint8_t cmd = anyOn ? 0 : 1;
    for (uint8_t z = 0; z < SystemState::MAX_ZONES; ++z) {
        _can.sendLightCommand(z, cmd);
        _can.stateMut().lightState[z] = (cmd == 1);
        _can.stateMut().lightValid[z] = true;
    }
    Serial.printf("[UI] Home: All lights %s\n", cmd ? "ON" : "OFF");
    _dirty = true;
}

// ============================================================================
// HVAC SCREEN
// ============================================================================

void UIManager::_drawHvacScreen()
{
    TFT_eSprite& spr = _display.getSprite();
    const SystemState& s = _can.state();

    int16_t y0 = UI_CONTENT_Y + 4;

    spr.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BACKGROUND);
    spr.setTextDatum(TL_DATUM);
    spr.setTextFont(FONT_MEDIUM);
    spr.setTextSize(1);
    spr.drawString("HVAC Control", 8, y0);
    _display.drawDivider(y0 + 28);

    // Current temperature display
    char buf[16];
    if (s.tempValid) {
        _ftoa(s.tempC, buf, 1);
    } else {
        strncpy(buf, "--.-", sizeof(buf));
    }
    _display.drawValueTile(4, y0 + 34, 148, 70, "Room Temp", buf, "\xB0""C");

    // Setpoint control
    int16_t spY = y0 + 34;
    _display.drawValueTile(158, spY, 154, 70, "Setpoint",
        [&]() -> const char* {
            _ftoa(_hvacSetpointEdit, buf, 1);
            return buf;
        }(), "\xB0""C", COLOR_ACCENT);

    // + / - buttons for setpoint
    int16_t btnY = spY + 72;
    _display.drawButton(158, btnY, 70, 30, "- 0.5", COLOR_BUTTON_NORMAL, COLOR_TEXT_PRIMARY);
    _display.drawButton(236, btnY, 76, 30, "+ 0.5", COLOR_BUTTON_NORMAL, COLOR_TEXT_PRIMARY);

    // Mode selection buttons
    int16_t modeY = btnY + 38;
    spr.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BACKGROUND);
    spr.setTextDatum(TL_DATUM);
    spr.setTextFont(FONT_SMALL);
    spr.setTextSize(1);
    spr.drawString("Mode:", 8, modeY);

    const char* modes[]   = { "Off", "Heat", "Cool", "Fan", "Auto" };
    const uint16_t modeColors[] = {
        COLOR_BUTTON_NORMAL,
        0xF480,  // Warm orange for heat
        COLOR_ACCENT,
        COLOR_BUTTON_NORMAL,
        COLOR_SUCCESS
    };
    for (uint8_t m = 0; m < 5; ++m) {
        bool active = (m == _hvacModeEdit);
        uint16_t bg = active ? modeColors[m] : COLOR_BUTTON_NORMAL;
        uint16_t fg = active ? TFT_WHITE : COLOR_TEXT_SECONDARY;
        _display.drawButton(4 + m * 62, modeY + 14, 58, 26, modes[m], bg, fg);
    }

    // Fan speed
    int16_t fanY = modeY + 48;
    spr.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BACKGROUND);
    spr.drawString("Fan:", 8, fanY);

    const char* fanSpeeds[] = { "Off", "Low", "Med", "High" };
    uint8_t currentFan = s.hvacValid ? s.hvacFanSpeed : 0;
    for (uint8_t f = 0; f < 4; ++f) {
        bool active = (f == currentFan);
        uint16_t bg = active ? COLOR_ACCENT : COLOR_BUTTON_NORMAL;
        uint16_t fg = active ? TFT_WHITE : COLOR_TEXT_SECONDARY;
        _display.drawButton(4 + f * 78, fanY + 14, 72, 26, fanSpeeds[f], bg, fg);
    }

    // Apply button
    _display.drawButton(96, fanY + 48, 128, 34, "Apply", COLOR_ACCENT, TFT_WHITE, false, 8);
}

void UIManager::_handleTouchHvac(const TouchPoint& pt, TouchEvent evt)
{
    if (evt != TouchEvent::PRESS && evt != TouchEvent::RELEASE) return;

    int16_t y0    = UI_CONTENT_Y + 4;
    int16_t spY   = y0 + 34;
    int16_t btnY  = spY + 72;
    int16_t modeY = btnY + 38;
    int16_t fanY  = modeY + 48;

    // Setpoint - button
    if (evt == TouchEvent::RELEASE &&
        TouchManager::hitTest(pt, 158, btnY, 70, 30)) {
        _hvacSetpointEdit = max(10.0f, _hvacSetpointEdit - 0.5f);
        _dirty = true;
    }
    // Setpoint + button
    if (evt == TouchEvent::RELEASE &&
        TouchManager::hitTest(pt, 236, btnY, 76, 30)) {
        _hvacSetpointEdit = min(32.0f, _hvacSetpointEdit + 0.5f);
        _dirty = true;
    }

    // Mode buttons
    for (uint8_t m = 0; m < 5; ++m) {
        if (evt == TouchEvent::RELEASE &&
            TouchManager::hitTest(pt, 4 + m * 62, modeY + 14, 58, 26)) {
            _hvacModeEdit = m;
            _dirty = true;
        }
    }

    // Fan speed (send immediately)
    for (uint8_t f = 0; f < 4; ++f) {
        if (evt == TouchEvent::RELEASE &&
            TouchManager::hitTest(pt, 4 + f * 78, fanY + 14, 72, 26)) {
            _can.sendHvacCommand(_hvacModeEdit, f, _hvacSetpointEdit);
            _can.stateMut().hvacFanSpeed = f;
            _dirty = true;
        }
    }

    // Apply button
    if (evt == TouchEvent::RELEASE &&
        TouchManager::hitTest(pt, 96, fanY + 48, 128, 34)) {
        _can.sendHvacCommand(_hvacModeEdit, _can.state().hvacFanSpeed, _hvacSetpointEdit);
        // Optimistic update
        _can.stateMut().hvacMode     = _hvacModeEdit;
        _can.stateMut().hvacSetpoint = _hvacSetpointEdit;
        _can.stateMut().hvacValid    = true;
        Serial.printf("[UI] HVAC: mode=%d fan=%d setpoint=%.1f\n",
                      _hvacModeEdit, _can.state().hvacFanSpeed, _hvacSetpointEdit);
        _dirty = true;
    }
}

// ============================================================================
// LIGHTS SCREEN
// ============================================================================

void UIManager::_drawLightsScreen()
{
    TFT_eSprite& spr = _display.getSprite();
    const SystemState& s = _can.state();

    int16_t y0 = UI_CONTENT_Y + 4;

    spr.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BACKGROUND);
    spr.setTextDatum(TL_DATUM);
    spr.setTextFont(FONT_MEDIUM);
    spr.setTextSize(1);
    spr.drawString("Lighting", 8, y0);
    _display.drawDivider(y0 + 28);

    const char* zoneNames[SystemState::MAX_ZONES] = {
        "Living Room", "Kitchen", "Bedroom", "Office",
        "Hallway",     "Garage",  "Patio",   "Bathroom"
    };

    int16_t rowY = y0 + 34;
    for (uint8_t z = 0; z < SystemState::MAX_ZONES; ++z) {
        int16_t col = z % 2;
        int16_t row = z / 2;
        int16_t bx  = 4 + col * 158;
        int16_t by  = rowY + row * 38;

        bool on      = s.lightValid[z] && s.lightState[z];
        bool pressed = (_pressedLightZone == z);

        _display.drawToggleButton(bx, by, 150, 32, zoneNames[z], on, pressed);

        // Brightness bar underneath when ON
        if (on && s.lightBrightness[z] > 0) {
            _display.drawProgressBar(bx, by + 33, 150, 4, s.lightBrightness[z], COLOR_ACCENT);
        }
    }

    // "All OFF" convenience button at bottom
    _display.drawButton(8, y0 + 175, 140, 32, "All Off",
                        COLOR_ERROR, TFT_WHITE, false, 6);
    _display.drawButton(156, y0 + 175, 156, 32, "All On",
                        COLOR_SUCCESS, TFT_WHITE, false, 6);
}

void UIManager::_handleTouchLights(const TouchPoint& pt, TouchEvent evt)
{
    const SystemState& s = _can.state();
    int16_t y0   = UI_CONTENT_Y + 4;
    int16_t rowY = y0 + 34;

    // Individual zone toggles
    for (uint8_t z = 0; z < SystemState::MAX_ZONES; ++z) {
        int16_t col = z % 2;
        int16_t row = z / 2;
        int16_t bx  = 4 + col * 158;
        int16_t by  = rowY + row * 38;

        if (TouchManager::hitTest(pt, bx, by, 150, 32)) {
            if (evt == TouchEvent::PRESS) {
                _pressedLightZone = z;
                _dirty = true;
            } else if (evt == TouchEvent::RELEASE && _pressedLightZone == z) {
                bool newState = !(s.lightValid[z] && s.lightState[z]);
                _can.sendLightCommand(z, newState ? 1 : 0);
                _can.stateMut().lightState[z] = newState;
                _can.stateMut().lightValid[z] = true;
                _pressedLightZone = 0xFF;
                _dirty = true;
                Serial.printf("[UI] Light zone %d -> %s\n", z, newState ? "ON" : "OFF");
            }
            return;
        }
    }

    // All Off button
    if (evt == TouchEvent::RELEASE &&
        TouchManager::hitTest(pt, 8, y0 + 175, 140, 32)) {
        for (uint8_t z = 0; z < SystemState::MAX_ZONES; ++z) {
            _can.sendLightCommand(z, 0);
            _can.stateMut().lightState[z] = false;
            _can.stateMut().lightValid[z] = true;
        }
        Serial.println("[UI] All lights OFF");
        _dirty = true;
    }

    // All On button
    if (evt == TouchEvent::RELEASE &&
        TouchManager::hitTest(pt, 156, y0 + 175, 156, 32)) {
        for (uint8_t z = 0; z < SystemState::MAX_ZONES; ++z) {
            _can.sendLightCommand(z, 1);
            _can.stateMut().lightState[z] = true;
            _can.stateMut().lightValid[z] = true;
        }
        Serial.println("[UI] All lights ON");
        _dirty = true;
    }

    if (evt == TouchEvent::RELEASE) {
        _pressedLightZone = 0xFF;
    }
}

// ============================================================================
// SETTINGS SCREEN
// ============================================================================

void UIManager::_drawSettingsScreen()
{
    TFT_eSprite& spr = _display.getSprite();

    int16_t y0 = UI_CONTENT_Y + 4;

    spr.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BACKGROUND);
    spr.setTextDatum(TL_DATUM);
    spr.setTextFont(FONT_MEDIUM);
    spr.setTextSize(1);
    spr.drawString("Settings", 8, y0);
    _display.drawDivider(y0 + 28);

    int16_t iy = y0 + 38;
    spr.setTextFont(FONT_SMALL);
    spr.setTextSize(1);

    // Firmware version
    spr.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BACKGROUND);
    spr.drawString("Firmware:", 8, iy);
    spr.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BACKGROUND);
    spr.drawString(FW_VERSION_STR, 100, iy);
    iy += 18;

    // CAN stats
    char buf[64];
    spr.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BACKGROUND);
    spr.drawString("CAN TX:", 8, iy);
    snprintf(buf, sizeof(buf), "%lu msgs", _can.txCount());
    spr.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BACKGROUND);
    spr.drawString(buf, 100, iy);
    iy += 18;

    spr.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BACKGROUND);
    spr.drawString("CAN RX:", 8, iy);
    snprintf(buf, sizeof(buf), "%lu msgs", _can.rxCount());
    spr.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BACKGROUND);
    spr.drawString(buf, 100, iy);
    iy += 18;

    spr.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BACKGROUND);
    spr.drawString("CAN ERR:", 8, iy);
    snprintf(buf, sizeof(buf), "%lu", _can.errCount());
    spr.setTextColor(_can.errCount() > 0 ? COLOR_ERROR : COLOR_SUCCESS, COLOR_BACKGROUND);
    spr.drawString(buf, 100, iy);
    iy += 18;

    spr.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BACKGROUND);
    spr.drawString("Bus:", 8, iy);
    bool busOff = _can.isBusOff();
    spr.setTextColor(busOff ? COLOR_ERROR : COLOR_SUCCESS, COLOR_BACKGROUND);
    spr.drawString(busOff ? "BUS OFF" : (_can.isRunning() ? "Active" : "Stopped"), 100, iy);
    iy += 24;

    _display.drawDivider(iy);
    iy += 8;

    // Backlight control
    spr.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BACKGROUND);
    spr.drawString("Backlight", 8, iy);
    uint8_t blPct = (uint8_t)(_display.getBrightness() * 100 / 255);
    _display.drawProgressBar(8, iy + 14, SCREEN_WIDTH - 16, 12, blPct, COLOR_ACCENT);
    _display.drawButton(8,   iy + 30, 60, 28, "Dim",    COLOR_BUTTON_NORMAL, COLOR_TEXT_PRIMARY);
    _display.drawButton(76,  iy + 30, 60, 28, "Medium", COLOR_BUTTON_NORMAL, COLOR_TEXT_PRIMARY);
    _display.drawButton(144, iy + 30, 60, 28, "Bright", COLOR_BUTTON_NORMAL, COLOR_TEXT_PRIMARY);
    iy += 64;

    // Touch calibration
    _display.drawButton(8, iy, 150, 30, "Calibrate Touch", COLOR_BUTTON_NORMAL, COLOR_TEXT_PRIMARY);
}

void UIManager::_handleTouchSettings(const TouchPoint& pt, TouchEvent evt)
{
    if (evt != TouchEvent::RELEASE) return;

    int16_t y0 = UI_CONTENT_Y + 4;
    // Backlight buttons row
    int16_t blY = y0 + 38 + 18*4 + 8 + 8 + 14 + 30;

    if (TouchManager::hitTest(pt, 8,   blY, 60, 28)) {
        _display.fadeTo(60, 200);
        _dirty = true;
    } else if (TouchManager::hitTest(pt, 76, blY, 60, 28)) {
        _display.fadeTo(150, 200);
        _dirty = true;
    } else if (TouchManager::hitTest(pt, 144, blY, 60, 28)) {
        _display.fadeTo(255, 200);
        _dirty = true;
    }

    // Touch calibration button
    int16_t calY = blY + 34;
    if (TouchManager::hitTest(pt, 8, calY, 150, 30)) {
        Serial.println("[UI] Starting touch calibration...");
        _touch.runCalibration(SCREEN_WIDTH, SCREEN_HEIGHT);
    }
}

// ============================================================================
// Utility helpers
// ============================================================================

void UIManager::_ftoa(float v, char* buf, uint8_t decimals)
{
    if (decimals == 0) {
        snprintf(buf, 12, "%d", (int)roundf(v));
    } else if (decimals == 1) {
        int i = (int)v;
        int f = abs((int)((v - i) * 10));
        snprintf(buf, 12, "%d.%d", i, f);
    } else {
        snprintf(buf, 12, "%.2f", v);
    }
}

const char* UIManager::_hvacModeLabel(uint8_t mode)
{
    switch (mode) {
        case 0: return "Off";
        case 1: return "Heat";
        case 2: return "Cool";
        case 3: return "Fan";
        case 4: return "Auto";
        default: return "???";
    }
}

const char* UIManager::_hvacFanLabel(uint8_t speed)
{
    switch (speed) {
        case 0: return "Off";
        case 1: return "Low";
        case 2: return "Med";
        case 3: return "High";
        default: return "???";
    }
}
