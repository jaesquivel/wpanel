#include <Arduino.h>
#include <SPI.h>

#include "../include/config.h"
#include "can/can_bus.h"
#include "display/display.h"
#include "touch/touch.h"
#include "ui/ui.h"

// ============================================================================
// Global subsystem instances
// ============================================================================
static DisplayManager displayMgr;
static TouchManager touchMgr;
static CANBusManager canMgr;
static UIManager uiMgr(displayMgr, touchMgr, canMgr);

// ============================================================================
// FreeRTOS task handles (optional — currently runs on single core)
// ============================================================================
// static TaskHandle_t canTaskHandle = nullptr;

// ============================================================================
// CAN heartbeat interval
// ============================================================================
static uint32_t lastHeartbeatMs = 0;
static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 2000;

static uint32_t lastBlinkMs = 0;
static bool blinkOn = false;
static constexpr uint32_t BLINK_INTERVAL_MS = 1000; // 0.5 Hz (1 s on, 1 s off)

// ============================================================================
// setup()
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(300); // Brief pause for serial monitor to connect
  Serial.println("\n\n============================");
  Serial.printf("  Wall Panel  v%s\n", FW_VERSION_STR);
  Serial.println("  ESP32-WROOM-32E");
  Serial.println("============================\n");

  // ---- RGB LED — hardware test blink (red flash confirms LED works) ----
  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  digitalWrite(LED_R_PIN, LOW); // RED on  (common anode: LOW = on)
  digitalWrite(LED_G_PIN, HIGH);
  digitalWrite(LED_B_PIN, HIGH);
  delay(300);
  digitalWrite(LED_R_PIN, HIGH); // all off

  // ---- Display ----
  if (!displayMgr.begin()) {
    Serial.println(
        "[Main] WARN: Display init failed — continuing without display");
  }

  // ---- Touch ----
  if (!touchMgr.begin()) {
    Serial.println(
        "[Main] WARN: Touch init failed — panel will be display-only");
  }

  // ---- CAN bus ----
  if (!canMgr.begin()) {
    Serial.println("[Main] WARN: CAN init failed — operating without bus");
  }

  // ---- UI ----
  uiMgr.begin();

  Serial.println("[Main] Boot complete — entering main loop\n");
}

// ============================================================================
// loop()
// ============================================================================
void loop() {
  uint32_t now = millis();

  // 1. Drain CAN RX queue and update system state
  canMgr.update();

  // 2. Run UI tick (processes touch, redraws if dirty)
  uiMgr.update();

  // 3. Periodic CAN heartbeat
  if ((now - lastHeartbeatMs) >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;

    uint8_t screenId = (uint8_t)uiMgr.currentScreen();
    uint8_t flags = 0x01; // bit0 = display_on
    canMgr.sendPanelStatus(screenId, flags);
  }

  // 4. RGB LED slow blink (white, 0.5 Hz — all channels together)
  // if ((now - lastBlinkMs) >= BLINK_INTERVAL_MS) {
  //     lastBlinkMs = now;
  //     blinkOn = !blinkOn;
  //     uint8_t level = blinkOn ? LOW : HIGH;  // common anode: LOW = on
  //     digitalWrite(LED_R_PIN, level);
  //     digitalWrite(LED_G_PIN, level);
  //     digitalWrite(LED_B_PIN, level);
  // }

  // 5. Yield to allow background tasks / watchdog
  yield();
}

// ============================================================================
// Optional: dedicated CAN task (uncomment to run on Core 0)
// ============================================================================
/*
void canTask(void* pvParameters)
{
    for (;;) {
        canMgr.update();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
*/
