#pragma once

#include <Arduino.h>
#include "../display/display.h"
#include "../touch/touch.h"
#include "../can/can_bus.h"
#include "../../include/config.h"

// ============================================================================
// Screen / page identifiers
// ============================================================================
enum class ScreenId : uint8_t {
    HOME       = 0,
    HVAC       = 1,
    LIGHTS     = 2,
    SETTINGS   = 3,
    COUNT
};

// ============================================================================
// UI layout constants
// ============================================================================
static constexpr int16_t UI_CONTENT_Y  = STATUS_BAR_HEIGHT + 2;
static constexpr int16_t UI_CONTENT_H  = SCREEN_HEIGHT - UI_CONTENT_Y - 30; // 30px nav bar

// Bottom navigation bar
static constexpr int16_t NAV_BAR_Y     = SCREEN_HEIGHT - 30;
static constexpr int16_t NAV_BAR_H     = 30;
static constexpr int16_t NAV_BTN_W     = SCREEN_WIDTH / (int)ScreenId::COUNT;

// ============================================================================
// UIManager — handles screens, navigation, and touch routing
// ============================================================================
class UIManager {
public:
    UIManager(DisplayManager& display, TouchManager& touch, CANBusManager& can);

    void begin();

    // Call once per loop — processes touch, redraws if dirty
    void update();

    // Navigate to a specific screen
    void navigateTo(ScreenId screen);

    ScreenId currentScreen() const { return _currentScreen; }

    // Force a full redraw on next update()
    void setDirty() { _dirty = true; }

private:
    DisplayManager& _display;
    TouchManager&   _touch;
    CANBusManager&  _can;

    ScreenId _currentScreen = ScreenId::HOME;
    bool     _dirty         = true;
    uint32_t _lastDrawMs    = 0;
    uint32_t _lastSensorMs  = 0;

    // Per-screen state
    uint8_t  _pressedNavBtn = 0xFF;   // Which nav button is being pressed

    // HVAC screen local state (pending changes before sending)
    float    _hvacSetpointEdit  = 22.0f;
    uint8_t  _hvacModeEdit      = 0;

    // Lights screen: which zone toggle is being pressed
    uint8_t  _pressedLightZone  = 0xFF;

    // Internal draw routines
    void _drawFrame();
    void _drawNavBar();

    void _drawHomeScreen();
    void _drawHvacScreen();
    void _drawLightsScreen();
    void _drawSettingsScreen();

    // Touch handling per screen
    void _handleTouchHome    (const TouchPoint& pt, TouchEvent evt);
    void _handleTouchHvac    (const TouchPoint& pt, TouchEvent evt);
    void _handleTouchLights  (const TouchPoint& pt, TouchEvent evt);
    void _handleTouchSettings(const TouchPoint& pt, TouchEvent evt);

    // Navigation bar touch check
    bool _handleNavBarTouch(const TouchPoint& pt, TouchEvent evt);

    // Helper: format float to "XX.X" string
    static void _ftoa(float v, char* buf, uint8_t decimals = 1);

    // Helper: HVAC mode label
    static const char* _hvacModeLabel(uint8_t mode);
    static const char* _hvacFanLabel(uint8_t speed);
};
