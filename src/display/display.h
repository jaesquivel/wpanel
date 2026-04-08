#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../../include/config.h"

// Forward declaration
class DisplayManager;

// Sprite/buffer identifiers
enum class SpriteId {
    MAIN_BUFFER,    // Full-screen double-buffer
    STATUS_BAR,     // Top status bar
    COUNT
};

class DisplayManager {
public:
    DisplayManager();

    // Lifecycle
    bool begin();
    void setBrightness(uint8_t brightness);   // 0–255
    void fadeTo(uint8_t targetBrightness, uint16_t durationMs = 300);

    // Direct TFT access (for drawing primitives)
    TFT_eSPI& tft() { return _tft; }

    // Double-buffered drawing helpers
    TFT_eSprite& getSprite() { return _screenBuf; }
    void pushBuffer();   // Blit _screenBuf to display

    // High-level draw primitives (draw into the sprite buffer)
    void fillScreen(uint16_t color);
    void drawStatusBar(const char* timeStr, bool canOk, int8_t rssi = 0);

    // Button drawing
    void drawButton(int16_t x, int16_t y, int16_t w, int16_t h,
                    const char* label, uint16_t bgColor, uint16_t textColor,
                    bool pressed = false, uint8_t radius = 6);

    // Toggle (on/off) button
    void drawToggleButton(int16_t x, int16_t y, int16_t w, int16_t h,
                          const char* label, bool state,
                          bool pressed = false, uint8_t radius = 6);

    // Value tile — shows a label + large numeric value + unit
    void drawValueTile(int16_t x, int16_t y, int16_t w, int16_t h,
                       const char* label, const char* value,
                       const char* unit, uint16_t valueColor = COLOR_TEXT_PRIMARY);

    // Slider bar (horizontal, read-only display)
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                         uint8_t percent, uint16_t fillColor = COLOR_ACCENT);

    // Draw a horizontal divider line
    void drawDivider(int16_t y, uint16_t color = COLOR_DIVIDER);

    // Screen dimensions
    uint16_t width() const  { return SCREEN_WIDTH; }
    uint16_t height() const { return SCREEN_HEIGHT; }

    // Backlight
    uint8_t getBrightness() const { return _brightness; }

private:
    TFT_eSPI   _tft;
    TFT_eSprite _screenBuf;
    uint8_t    _brightness;

    void _initBacklight();
    void _setBLDuty(uint8_t duty);
};
