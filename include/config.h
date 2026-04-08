#pragma once

// =============================================================================
// Wall Control Panel - Hardware Configuration
// Board: ESP32-2432S028R v3 (Cheap Yellow Display / CYD)
//        ESP32-WROOM-32 + ILI9341 2.8" TFT + XPT2046 touch + RGB LED
// =============================================================================

// -----------------------------------------------------------------------------
// SPI Bus - TFT display (HSPI: SCK=14, MISO=12, MOSI=13)
// Touch controller uses its own dedicated SPI pins (CLK=25, DIN=32)
// -----------------------------------------------------------------------------
#define SPI_MOSI_PIN 13
#define SPI_MISO_PIN 12
#define SPI_CLK_PIN 14

// -----------------------------------------------------------------------------
// ILI9341 TFT Display (SPI)
// -----------------------------------------------------------------------------
#define TFT_CS_PIN 15
#define TFT_DC_PIN 2
#define TFT_RST_PIN -1   // RST tied to EN (hardware reset), not a GPIO
#define TFT_BL_PIN 21    // Backlight PWM control (GPIO21)
#define TFT_BL_FREQ 5000 // Backlight PWM frequency (Hz)
#define TFT_BL_CHANNEL 0 // LEDC channel for backlight

// Screen resolution
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define SCREEN_ROTATION                                                        \
  1 // Landscape: 0=portrait, 1=landscape, 2=portrait flipped, 3=landscape
    // flipped

// Default backlight brightness (0-255)
#define TFT_BL_DEFAULT 100

// -----------------------------------------------------------------------------
// RGB LED (common anode: LOW = on, HIGH = off)
// Note: GPIO16/17 are also PSRAM pins — only safe if PSRAM is not fitted
// -----------------------------------------------------------------------------
#define LED_R_PIN 4
#define LED_G_PIN 16
#define LED_B_PIN 17

// -----------------------------------------------------------------------------
// XPT2046 Resistive Touch Controller (dedicated VSPI bus, separate from TFT)
// -----------------------------------------------------------------------------
#define TOUCH_CLK_PIN 25
#define TOUCH_MOSI_PIN 32
#define TOUCH_MISO_PIN 39 // GPIO39 input-only (SENSOR_VN)
#define TOUCH_CS_PIN 33
#define TOUCH_IRQ_PIN 36 // GPIO36 input-only (SENSOR_VP), no pull-up needed

// Touch calibration constants (raw ADC values from XPT2046)
// Calibrate by reading raw values at known screen corners
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3800
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3800

// Touch pressure threshold (0-4095, lower = lighter touch)
#define TOUCH_PRESSURE_MIN 200

// Touch debounce - minimum ms between registered touch events
#define TOUCH_DEBOUNCE_MS 80

// -----------------------------------------------------------------------------
// MicroSD Card (VSPI bus — separate from TFT HSPI and touch)
// -----------------------------------------------------------------------------
#define SD_CLK_PIN 18
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_CS_PIN 5

// -----------------------------------------------------------------------------
// Onboard peripherals
// -----------------------------------------------------------------------------
#define LDR_PIN 34     // Light-dependent resistor (ADC, input-only)
#define SPEAKER_PIN 26 // Passive speaker / audio out

// -----------------------------------------------------------------------------
// CAN Bus - ESP32 TWAI peripheral (built-in CAN controller)
// Uses SN65HVD230 3.3V CAN transceiver wired to the P3/CN1 expansion headers
// WARNING: CAN_TX_PIN (GPIO5) = SD_CS_PIN — do not use SD and CAN together
// -----------------------------------------------------------------------------
#define CAN_TX_PIN 5  // GPIO5  -> SN65HVD230 TXD  (shared with SD_CS_PIN)
#define CAN_RX_PIN 35 // GPIO35 -> SN65HVD230 RXD (input-only)

// CAN bus speed
#define CAN_BAUD_RATE TWAI_TIMING_CONFIG_500KBITS()

// CAN message IDs for wall panel protocol
#define CAN_ID_PANEL_STATUS 0x100  // Panel broadcasts its status
#define CAN_ID_PANEL_COMMAND 0x101 // Panel sends commands (e.g., light on/off)
#define CAN_ID_SENSOR_TEMP 0x200   // Temperature sensor data
#define CAN_ID_SENSOR_HUMIDITY 0x201 // Humidity sensor data
#define CAN_ID_HVAC_STATUS 0x300     // HVAC system status
#define CAN_ID_HVAC_COMMAND 0x301    // HVAC commands from panel
#define CAN_ID_LIGHT_STATUS 0x400    // Lighting system status
#define CAN_ID_LIGHT_COMMAND 0x401   // Lighting commands from panel

// CAN transmit queue depth
#define CAN_TX_QUEUE_LEN 10
#define CAN_RX_QUEUE_LEN 20

// CAN watchdog: if no message received in this time, show warning
#define CAN_TIMEOUT_MS 5000

// -----------------------------------------------------------------------------
// UI / Application settings
// -----------------------------------------------------------------------------
#define UI_UPDATE_INTERVAL_MS 50     // ~20 FPS UI refresh rate
#define SENSOR_POLL_INTERVAL_MS 1000 // Re-draw sensor values every 1s
#define STATUS_BAR_HEIGHT 20         // Pixels for top status bar

// Color palette (RGB565)
#define COLOR_BACKGROUND 0x000C     // Very dark blue  (R=0 G=0 B=12/31)
#define COLOR_PANEL_BG   0x000C     // Very dark blue
#define COLOR_ACCENT 0x04FF         // Teal/cyan accent
#define COLOR_TEXT_PRIMARY 0xFFFF   // White
#define COLOR_TEXT_SECONDARY 0xC618 // Light grey
#define COLOR_TEXT_DIM 0x7BEF       // Mid grey
#define COLOR_SUCCESS 0x07E0        // Green
#define COLOR_WARNING 0xFFE0        // Yellow
#define COLOR_ERROR 0xF800          // Red
#define COLOR_BUTTON_NORMAL 0x2945  // Button idle
#define COLOR_BUTTON_PRESS 0x04FF   // Button pressed (accent)
#define COLOR_BUTTON_OFF 0x39E7     // Toggle OFF state
#define COLOR_BUTTON_ON 0x04FF      // Toggle ON state (accent)
#define COLOR_DIVIDER 0x4208        // Subtle divider line

// Font sizes used (TFT_eSPI built-in fonts)
#define FONT_SMALL 2  // ~10px
#define FONT_MEDIUM 4 // ~26px
#define FONT_LARGE 6  // Numbers/clock

// -----------------------------------------------------------------------------
// Firmware version
// -----------------------------------------------------------------------------
#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0
#define FW_VERSION_STR "1.0.0"
