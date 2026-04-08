#include "can_bus.h"
#include <cstring>

// ============================================================================
// CANBusManager implementation — uses ESP32 TWAI (built-in CAN peripheral)
// Physical layer: SN65HVD230 3.3V transceiver
// ============================================================================

CANBusManager::CANBusManager() {}

// ----------------------------------------------------------------------------
// begin() — configure and start the TWAI driver
// ----------------------------------------------------------------------------
bool CANBusManager::begin()
{
    // General configuration
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN,
        (gpio_num_t)CAN_RX_PIN,
        TWAI_MODE_NORMAL
    );
    g_config.tx_queue_len = CAN_TX_QUEUE_LEN;
    g_config.rx_queue_len = CAN_RX_QUEUE_LEN;

    // Timing: 500 kbps
    twai_timing_config_t t_config = CAN_BAUD_RATE;

    // Acceptance filter — accept ALL messages (we filter by ID in software)
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install driver
    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        Serial.printf("[CAN] Driver install failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // Start driver
    err = twai_start();
    if (err != ESP_OK) {
        Serial.printf("[CAN] Start failed: %s\n", esp_err_to_name(err));
        twai_driver_uninstall();
        return false;
    }

    _running  = true;
    _lastRxMs = millis();
    Serial.printf("[CAN] TWAI started  TX=GPIO%d  RX=GPIO%d  500kbps\n",
                  CAN_TX_PIN, CAN_RX_PIN);
    return true;
}

// ----------------------------------------------------------------------------
// stop()
// ----------------------------------------------------------------------------
void CANBusManager::stop()
{
    if (_running) {
        twai_stop();
        twai_driver_uninstall();
        _running = false;
        Serial.println("[CAN] TWAI stopped");
    }
}

// ----------------------------------------------------------------------------
// update() — drain the RX queue (call each loop iteration or from a task)
// ----------------------------------------------------------------------------
void CANBusManager::update()
{
    if (!_running) return;

    twai_message_t msg;
    // Drain up to 20 messages per call to avoid blocking too long
    for (uint8_t i = 0; i < 20; ++i) {
        esp_err_t err = twai_receive(&msg, 0);   // Non-blocking (timeout=0)
        if (err == ESP_ERR_TIMEOUT) break;        // Queue empty
        if (err != ESP_OK) {
            _errCount++;
            continue;
        }
        _rxCount++;
        _lastRxMs = millis();
        _processMessage(msg);
    }

    // Check for bus-off recovery
    if (isBusOff()) {
        Serial.println("[CAN] Bus-off detected — attempting recovery");
        twai_initiate_recovery();
    }
}

// ----------------------------------------------------------------------------
// isBusOff()
// ----------------------------------------------------------------------------
bool CANBusManager::isBusOff() const
{
    if (!_running) return false;
    twai_status_info_t status;
    if (twai_get_status_info(&status) != ESP_OK) return false;
    return (status.state == TWAI_STATE_BUS_OFF);
}

// ----------------------------------------------------------------------------
// _processMessage() — dispatch by CAN ID
// ----------------------------------------------------------------------------
void CANBusManager::_processMessage(const twai_message_t& msg)
{
    if (msg.rtr) return;   // Ignore remote frames

    switch (msg.identifier) {
        case CAN_ID_SENSOR_TEMP:
            _handleSensorTemp(msg);
            break;
        case CAN_ID_SENSOR_HUMIDITY:
            _handleSensorHumidity(msg);
            break;
        case CAN_ID_HVAC_STATUS:
            _handleHvacStatus(msg);
            break;
        case CAN_ID_LIGHT_STATUS:
            _handleLightStatus(msg);
            break;
        default:
            // Unknown / unhandled ID — log at debug level
            Serial.printf("[CAN] RX id=0x%03X len=%d  [unhandled]\n",
                          msg.identifier, msg.data_length_code);
            break;
    }
}

// ----------------------------------------------------------------------------
// Message handlers
// ----------------------------------------------------------------------------

void CANBusManager::_handleSensorTemp(const twai_message_t& msg)
{
    if (msg.data_length_code < sizeof(MsgSensorTemp)) return;

    MsgSensorTemp pkt;
    memcpy(&pkt, msg.data, sizeof(pkt));

    if (pkt.flags & 0x01) {  // Valid bit
        _state.tempC     = pkt.tempC_x10 / 10.0f;
        _state.tempValid = true;
        _state.tempLastMs = millis();
    }
}

void CANBusManager::_handleSensorHumidity(const twai_message_t& msg)
{
    if (msg.data_length_code < sizeof(MsgSensorHumidity)) return;

    MsgSensorHumidity pkt;
    memcpy(&pkt, msg.data, sizeof(pkt));

    if (pkt.flags & 0x01) {
        _state.humidityPct  = pkt.humidityPct_x10 / 10.0f;
        _state.humidValid   = true;
        _state.humidLastMs  = millis();
    }
}

void CANBusManager::_handleHvacStatus(const twai_message_t& msg)
{
    if (msg.data_length_code < sizeof(MsgHvacStatus)) return;

    MsgHvacStatus pkt;
    memcpy(&pkt, msg.data, sizeof(pkt));

    _state.hvacMode     = pkt.mode;
    _state.hvacFanSpeed = pkt.fanSpeed;
    _state.hvacSetpoint = pkt.setpointC_x10 / 10.0f;
    _state.hvacActual   = pkt.actualC_x10   / 10.0f;
    _state.hvacValid    = true;
    _state.hvacLastMs   = millis();
}

void CANBusManager::_handleLightStatus(const twai_message_t& msg)
{
    if (msg.data_length_code < sizeof(MsgLightStatus)) return;

    MsgLightStatus pkt;
    memcpy(&pkt, msg.data, sizeof(pkt));

    if (pkt.zoneId < SystemState::MAX_ZONES) {
        _state.lightState[pkt.zoneId]      = (pkt.state != 0);
        _state.lightBrightness[pkt.zoneId] = pkt.brightness;
        _state.lightValid[pkt.zoneId]      = true;
    }
}

// ----------------------------------------------------------------------------
// sendRaw()
// ----------------------------------------------------------------------------
bool CANBusManager::sendRaw(uint32_t id, const uint8_t* data, uint8_t len,
                             bool extFrame, uint32_t timeoutMs)
{
    if (!_running) return false;

    twai_message_t msg = {};
    msg.identifier         = id;
    msg.extd               = extFrame ? 1 : 0;
    msg.rtr                = 0;
    msg.data_length_code   = min(len, (uint8_t)8);
    memcpy(msg.data, data, msg.data_length_code);

    TickType_t ticks = pdMS_TO_TICKS(timeoutMs);
    esp_err_t err = twai_transmit(&msg, ticks);
    if (err == ESP_OK) {
        _txCount++;
        return true;
    }
    Serial.printf("[CAN] TX failed id=0x%03X err=%s\n", id, esp_err_to_name(err));
    return false;
}

// ----------------------------------------------------------------------------
// High-level transmit helpers
// ----------------------------------------------------------------------------

bool CANBusManager::sendPanelStatus(uint8_t screenId, uint8_t flags)
{
    MsgPanelStatus pkt = {};
    pkt.fwMajor  = FW_VERSION_MAJOR;
    pkt.fwMinor  = FW_VERSION_MINOR;
    pkt.screenId = screenId;
    pkt.flags    = flags;
    return sendRaw(CAN_ID_PANEL_STATUS, (uint8_t*)&pkt, sizeof(pkt));
}

bool CANBusManager::sendLightCommand(uint8_t zoneId, uint8_t command, uint8_t value)
{
    MsgPanelCommand pkt = {};
    pkt.targetId = zoneId;
    pkt.command  = command;
    pkt.value    = value;
    return sendRaw(CAN_ID_LIGHT_COMMAND, (uint8_t*)&pkt, sizeof(pkt));
}

bool CANBusManager::sendHvacCommand(uint8_t mode, uint8_t fanSpeed, float setpointC)
{
    // Reuse MsgHvacStatus layout for the command (setpoint + mode fields)
    MsgHvacStatus pkt = {};
    pkt.mode            = mode;
    pkt.fanSpeed        = fanSpeed;
    pkt.setpointC_x10   = (int16_t)(setpointC * 10.0f);
    pkt.actualC_x10     = 0;
    return sendRaw(CAN_ID_HVAC_COMMAND, (uint8_t*)&pkt, sizeof(pkt));
}
