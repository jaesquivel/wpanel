#include "display.h"

// ============================================================================
// DisplayManager implementation
// ============================================================================

DisplayManager::DisplayManager()
    : _tft(), _screenBuf(&_tft), _brightness(TFT_BL_DEFAULT) {}

bool DisplayManager::begin() {
  // Initialise TFT driver
  _tft.init();
  _tft.invertDisplay(false);  // ST7789 init sends INVON by default; undo it
  _tft.setRotation(SCREEN_ROTATION);
  // ST7789 driver hardcodes BGR bit in MADCTL; this panel uses RGB order.
  // 0x60 = MX(0x40) | MV(0x20) = landscape, RGB order, BGR bit cleared.
  _tft.writecommand(0x36);
  _tft.writedata(0x60);
  _tft.fillScreen(TFT_BLACK);

  // Create a full-screen sprite for double-buffering.
  // Try 16-bit first (~150 KB); fall back to 8-bit (~75 KB) if DRAM is tight.
  _screenBuf.setColorDepth(16);
  void *ptr = _screenBuf.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  if (!ptr) {
    Serial.printf(
        "[Display] 16-bit sprite failed (free heap: %lu B) — trying 8-bit\n",
        (unsigned long)ESP.getFreeHeap());
    _screenBuf.setColorDepth(8);
    ptr = _screenBuf.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  }
  Serial.printf("[Display] Sprite alloc %s  depth=%d  free heap: %lu B\n",
                ptr ? "OK" : "FAILED", ptr ? _screenBuf.getColorDepth() : 0,
                (unsigned long)ESP.getFreeHeap());
  if (!ptr) {
    Serial.println("[Display] FATAL: not enough DRAM even for 8-bit buffer");
    return false;
  }
  _screenBuf.fillSprite(COLOR_BACKGROUND);

  // Backlight
  _initBacklight();
  fadeTo(TFT_BL_DEFAULT, 400);

  Serial.printf("[Display] Init OK  %dx%d  rotation=%d\n", SCREEN_WIDTH,
                SCREEN_HEIGHT, SCREEN_ROTATION);
  return true;
}

// ----------------------------------------------------------------------------
// Backlight
// ----------------------------------------------------------------------------

void DisplayManager::_initBacklight() {
  ledcSetup(TFT_BL_CHANNEL, TFT_BL_FREQ, 8); // 8-bit resolution (0-255)
  ledcAttachPin(TFT_BL_PIN, TFT_BL_CHANNEL);
  _setBLDuty(0);
}

void DisplayManager::_setBLDuty(uint8_t duty) {
  ledcWrite(TFT_BL_CHANNEL, duty);
  _brightness = duty;
}

void DisplayManager::setBrightness(uint8_t brightness) {
  _setBLDuty(brightness);
}

void DisplayManager::fadeTo(uint8_t targetBrightness, uint16_t durationMs) {
  int16_t current = _brightness;
  int16_t target = targetBrightness;
  int16_t delta = target - current;
  if (delta == 0)
    return;

  uint16_t steps = abs(delta);
  uint16_t delayMs = max(1u, (uint32_t)durationMs / steps);

  for (int16_t i = 0; i <= steps; ++i) {
    uint8_t val = (uint8_t)(current + (delta * i) / steps);
    _setBLDuty(val);
    delay(delayMs);
  }
  _setBLDuty(targetBrightness);
}

// ----------------------------------------------------------------------------
// Buffer helpers
// ----------------------------------------------------------------------------

void DisplayManager::fillScreen(uint16_t color) {
  _screenBuf.fillSprite(color);
}

void DisplayManager::pushBuffer() { _screenBuf.pushSprite(0, 0); }

// ----------------------------------------------------------------------------
// Status bar
// ----------------------------------------------------------------------------

void DisplayManager::drawStatusBar(const char *timeStr, bool canOk,
                                   int8_t /*rssi*/) {
  const int16_t barY = 0;
  const int16_t barH = STATUS_BAR_HEIGHT;

  _screenBuf.fillRect(0, barY, SCREEN_WIDTH, barH, COLOR_PANEL_BG);

  // Left: firmware / panel label
  _screenBuf.setTextColor(COLOR_TEXT_DIM, COLOR_PANEL_BG);
  _screenBuf.setTextDatum(ML_DATUM);
  _screenBuf.setTextFont(1);
  _screenBuf.setTextSize(1);
  _screenBuf.drawString("WPanel v" FW_VERSION_STR, 6, barY + barH / 2);

  // Centre: time string
  if (timeStr && timeStr[0]) {
    _screenBuf.setTextColor(COLOR_TEXT_PRIMARY, COLOR_PANEL_BG);
    _screenBuf.setTextDatum(MC_DATUM);
    _screenBuf.drawString(timeStr, SCREEN_WIDTH / 2, barY + barH / 2);
  }

  // Right: CAN bus status indicator
  _screenBuf.setTextDatum(MR_DATUM);
  uint16_t indicatorColor = canOk ? COLOR_SUCCESS : COLOR_ERROR;
  const char *canLabel = canOk ? "CAN" : "!CAN";
  _screenBuf.setTextColor(indicatorColor, COLOR_PANEL_BG);
  _screenBuf.drawString(canLabel, SCREEN_WIDTH - 6, barY + barH / 2);

  // Bottom divider
  _screenBuf.drawFastHLine(0, barY + barH - 1, SCREEN_WIDTH, COLOR_DIVIDER);
}

// ----------------------------------------------------------------------------
// Button
// ----------------------------------------------------------------------------

void DisplayManager::drawButton(int16_t x, int16_t y, int16_t w, int16_t h,
                                const char *label, uint16_t bgColor,
                                uint16_t textColor, bool pressed,
                                uint8_t radius) {
  uint16_t bg = pressed ? (uint16_t)(bgColor >> 1) // Darken when pressed
                        : bgColor;

  _screenBuf.fillRoundRect(x, y, w, h, radius, bg);
  _screenBuf.drawRoundRect(x, y, w, h, radius,
                           pressed ? COLOR_ACCENT : COLOR_DIVIDER);

  // Shadow effect (subtle bottom-right lines when not pressed)
  if (!pressed) {
    _screenBuf.drawFastHLine(x + radius, y + h, w - radius, COLOR_DIVIDER);
    _screenBuf.drawFastVLine(x + w, y + radius, h - radius, COLOR_DIVIDER);
  }

  _screenBuf.setTextDatum(MC_DATUM);
  _screenBuf.setTextColor(textColor, bg);
  _screenBuf.setTextFont(FONT_SMALL);
  _screenBuf.setTextSize(1);
  _screenBuf.drawString(label, x + w / 2, y + h / 2);
}

// ----------------------------------------------------------------------------
// Toggle button
// ----------------------------------------------------------------------------

void DisplayManager::drawToggleButton(int16_t x, int16_t y, int16_t w,
                                      int16_t h, const char *label, bool state,
                                      bool pressed, uint8_t radius) {
  uint16_t bg = state ? COLOR_BUTTON_ON : COLOR_BUTTON_OFF;
  uint16_t txt = state ? TFT_WHITE : COLOR_TEXT_SECONDARY;
  uint16_t border = state ? COLOR_ACCENT : COLOR_DIVIDER;

  if (pressed)
    bg = (uint16_t)(bg >> 1);

  _screenBuf.fillRoundRect(x, y, w, h, radius, bg);
  _screenBuf.drawRoundRect(x, y, w, h, radius, border);

  // Small dot indicator
  uint16_t dotColor = state ? COLOR_SUCCESS : COLOR_TEXT_DIM;
  _screenBuf.fillCircle(x + 12, y + h / 2, 4, dotColor);

  _screenBuf.setTextDatum(ML_DATUM);
  _screenBuf.setTextColor(txt, bg);
  _screenBuf.setTextFont(FONT_SMALL);
  _screenBuf.setTextSize(1);
  _screenBuf.drawString(label, x + 24, y + h / 2);

  // State text (right aligned)
  _screenBuf.setTextDatum(MR_DATUM);
  _screenBuf.setTextColor(state ? COLOR_SUCCESS : COLOR_TEXT_DIM, bg);
  _screenBuf.drawString(state ? "ON" : "OFF", x + w - 8, y + h / 2);
}

// ----------------------------------------------------------------------------
// Value tile
// ----------------------------------------------------------------------------

void DisplayManager::drawValueTile(int16_t x, int16_t y, int16_t w, int16_t h,
                                   const char *label, const char *value,
                                   const char *unit, uint16_t valueColor) {
  _screenBuf.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
  _screenBuf.drawRoundRect(x, y, w, h, 6, COLOR_DIVIDER);

  // Label at top
  _screenBuf.setTextDatum(TL_DATUM);
  _screenBuf.setTextColor(COLOR_TEXT_SECONDARY, COLOR_PANEL_BG);
  _screenBuf.setTextFont(FONT_SMALL);
  _screenBuf.setTextSize(1);
  _screenBuf.drawString(label, x + 6, y + 5);

  // Value — large, centred vertically
  _screenBuf.setTextDatum(MC_DATUM);
  _screenBuf.setTextColor(valueColor, COLOR_PANEL_BG);
  _screenBuf.setTextFont(FONT_LARGE);
  _screenBuf.setTextSize(1);
  _screenBuf.drawString(value, x + w / 2, y + h / 2 + 4);

  // Unit — bottom right
  _screenBuf.setTextDatum(BR_DATUM);
  _screenBuf.setTextColor(COLOR_TEXT_DIM, COLOR_PANEL_BG);
  _screenBuf.setTextFont(FONT_SMALL);
  _screenBuf.setTextSize(1);
  _screenBuf.drawString(unit, x + w - 6, y + h - 5);
}

// ----------------------------------------------------------------------------
// Progress bar
// ----------------------------------------------------------------------------

void DisplayManager::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                                     uint8_t percent, uint16_t fillColor) {
  percent = min(percent, (uint8_t)100);
  _screenBuf.drawRect(x, y, w, h, COLOR_DIVIDER);
  _screenBuf.fillRect(x + 1, y + 1, w - 2, h - 2, COLOR_PANEL_BG);

  int16_t fillW = (int16_t)((w - 2) * percent / 100);
  if (fillW > 0) {
    _screenBuf.fillRect(x + 1, y + 1, fillW, h - 2, fillColor);
  }
}

// ----------------------------------------------------------------------------
// Divider
// ----------------------------------------------------------------------------

void DisplayManager::drawDivider(int16_t y, uint16_t color) {
  _screenBuf.drawFastHLine(8, y, SCREEN_WIDTH - 16, color);
}
