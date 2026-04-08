#pragma once

#include <Arduino.h>
#include <driver/twai.h>       // ESP32 built-in TWAI (CAN) driver
#include "../../include/config.h"

// ============================================================================
// CAN message payload structures (little-endian packing)
// ============================================================================

// 0x100 — Panel heartbeat / status
struct __attribute__((packed)) MsgPanelStatus {
    uint8_t  fwMajor;
    uint8_t  fwMinor;
    uint8_t  screenId;      // Active UI screen
    uint8_t  flags;         // bit0=display_on, bit1=touch_active
};

// 0x101 — Panel command (light / fan / general relay)
struct __attribute__((packed)) MsgPanelCommand {
    uint8_t  targetId;      // Which device to control
    uint8_t  command;       // 0=off, 1=on, 2=toggle, 3=set_level
    uint8_t  value;         // Level 0-100 for dimmer; ignored for on/off
    uint8_t  reserved;
};

// 0x200 — Temperature reading
struct __attribute__((packed)) MsgSensorTemp {
    int16_t  tempC_x10;    // Temperature * 10 (e.g. 235 = 23.5°C)
    uint8_t  sensorId;
    uint8_t  flags;        // bit0=valid
};

// 0x201 — Humidity reading
struct __attribute__((packed)) MsgSensorHumidity {
    uint16_t humidityPct_x10; // Humidity % * 10
    uint8_t  sensorId;
    uint8_t  flags;
};

// 0x300 — HVAC status
struct __attribute__((packed)) MsgHvacStatus {
    uint8_t  mode;         // 0=off, 1=heat, 2=cool, 3=fan, 4=auto
    uint8_t  fanSpeed;     // 0-3 (off/low/med/high)
    int16_t  setpointC_x10;
    int16_t  actualC_x10;
    uint8_t  flags;        // bit0=compressor_on, bit1=fan_on
    uint8_t  reserved;
};

// 0x400 — Light zone status
struct __attribute__((packed)) MsgLightStatus {
    uint8_t  zoneId;
    uint8_t  state;        // 0=off, 1=on
    uint8_t  brightness;   // 0-100 %
    uint8_t  reserved;
};

// ============================================================================
// Parsed / aggregated state from CAN bus
// ============================================================================
struct SystemState {
    // Temperature / humidity (from last CAN messages)
    float    tempC         = 0.0f;
    float    humidityPct   = 0.0f;
    bool     tempValid     = false;
    bool     humidValid    = false;
    uint32_t tempLastMs    = 0;
    uint32_t humidLastMs   = 0;

    // HVAC
    uint8_t  hvacMode      = 0;     // 0=off
    uint8_t  hvacFanSpeed  = 0;
    float    hvacSetpoint  = 22.0f;
    float    hvacActual    = 0.0f;
    bool     hvacValid     = false;
    uint32_t hvacLastMs    = 0;

    // Lights (up to 8 zones)
    static constexpr uint8_t MAX_ZONES = 8;
    bool     lightState[MAX_ZONES]      = {};
    uint8_t  lightBrightness[MAX_ZONES] = {};
    bool     lightValid[MAX_ZONES]      = {};
};

// ============================================================================
// CANBusManager
// ============================================================================
class CANBusManager {
public:
    CANBusManager();

    // Lifecycle
    bool begin();
    void stop();

    // Call from main loop (or dedicated task)
    // Drains RX queue and updates internal SystemState
    void update();

    // Transmit helpers (non-blocking — queues to TWAI TX)
    bool sendPanelStatus(uint8_t screenId, uint8_t flags);
    bool sendLightCommand(uint8_t zoneId, uint8_t command, uint8_t value = 0);
    bool sendHvacCommand(uint8_t mode, uint8_t fanSpeed, float setpointC);

    // Send a raw TWAI frame (returns true if queued OK)
    bool sendRaw(uint32_t id, const uint8_t* data, uint8_t len,
                 bool extFrame = false, uint32_t timeoutMs = 10);

    // State access
    const SystemState& state() const { return _state; }
    SystemState&       stateMut()    { return _state; }

    bool isRunning()  const { return _running; }
    bool isBusOff()   const;
    uint32_t rxCount() const { return _rxCount; }
    uint32_t txCount() const { return _txCount; }
    uint32_t errCount() const { return _errCount; }

    // Last successful RX timestamp (for timeout detection)
    uint32_t lastRxMs() const { return _lastRxMs; }

private:
    SystemState _state;
    bool        _running  = false;
    uint32_t    _rxCount  = 0;
    uint32_t    _txCount  = 0;
    uint32_t    _errCount = 0;
    uint32_t    _lastRxMs = 0;

    void _processMessage(const twai_message_t& msg);
    void _handleSensorTemp    (const twai_message_t& msg);
    void _handleSensorHumidity(const twai_message_t& msg);
    void _handleHvacStatus    (const twai_message_t& msg);
    void _handleLightStatus   (const twai_message_t& msg);
};
