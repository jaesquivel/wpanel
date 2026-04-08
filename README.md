# Wall Panel — ESP32 CAN Bus Control Panel

A wall-mounted home control panel built on the **ESP32-2432S028R v3** (Cheap Yellow Display), featuring an ILI9341 320×240 TFT display with XPT2046 resistive touch and CAN bus communication via the ESP32's built-in TWAI peripheral.

---

## Hardware

| Component | Part | Interface |
|---|---|---|
| MCU | ESP32-WROOM-32 (on ESP32-2432S028R v3) | — |
| Display | ILI9341 2.8" TFT (320×240) | SPI (HSPI) |
| Touch | XPT2046 resistive controller | SPI (VSPI, dedicated) |
| RGB LED | Common-anode, 3-channel | GPIO (active LOW) |
| MicroSD | — | SPI (VSPI, dedicated) |
| LDR | Light sensor | ADC (GPIO 34) |
| Speaker | Passive | GPIO 26 |
| CAN transceiver | SN65HVD230 (external, 3.3V) | TWAI peripheral |

### Pin assignments

#### TFT Display — ILI9341 (HSPI)

| Signal | GPIO | Notes |
|---|---|---|
| MOSI (SDI) | 13 | |
| MISO (SDO) | 12 | |
| SCK | 14 | |
| CS | 15 | |
| DC (RS) | 2 | |
| RST | — | Tied to EN (hardware reset) |
| Backlight | 21 | PWM via LEDC channel 0 |

#### Touch Controller — XPT2046 (VSPI)

| Signal | GPIO | Notes |
|---|---|---|
| MOSI (DIN) | 32 | |
| MISO (DO) | 39 | Input-only (SENSOR_VN) |
| CLK | 25 | |
| CS | 33 | |
| IRQ | 36 | Input-only (SENSOR_VP), no pull-up |

#### MicroSD Card (VSPI)

| Signal | GPIO | Notes |
|---|---|---|
| MOSI | 23 | |
| MISO | 19 | |
| CLK | 18 | |
| CS | 5 | ⚠️ Shared with CAN TX — cannot use SD and CAN simultaneously |

#### RGB LED (common anode — active LOW)

| Channel | GPIO |
|---|---|
| Red | 4 |
| Green | 16 |
| Blue | 17 |

#### Onboard peripherals

| Peripheral | GPIO | Notes |
|---|---|---|
| LDR | 34 | ADC, input-only |
| Speaker | 26 | Passive buzzer / audio |
| Boot button | 0 | |

#### CAN Bus — TWAI + external SN65HVD230

| Signal | GPIO | Notes |
|---|---|---|
| TX | 5 | ⚠️ Shared with SD CS — do not use SD and CAN simultaneously |
| RX | 35 | Input-only |

---

## Software architecture

```
src/
  main.cpp              Entry point: setup/loop, subsystem wiring
  display/
    display.h/.cpp      DisplayManager — TFT init, sprite buffering, draw primitives
  touch/
    touch.h/.cpp        TouchManager — XPT2046, calibration, event detection
  can/
    can_bus.h/.cpp      CANBusManager — TWAI driver, message parsing, TX helpers
  ui/
    ui.h/.cpp           UIManager — screen routing, home/HVAC/lights/settings screens
include/
  config.h              All pin definitions, CAN IDs, color palette, constants
```

### CAN bus message protocol (500 kbps)

| CAN ID | Direction | Description |
|---|---|---|
| 0x100 | TX (panel) | Panel heartbeat / status |
| 0x101 | TX (panel) | Generic command (light, fan, relay) |
| 0x200 | RX | Temperature sensor reading |
| 0x201 | RX | Humidity sensor reading |
| 0x300 | RX | HVAC system status |
| 0x301 | TX (panel) | HVAC command (mode, fan, setpoint) |
| 0x400 | RX | Light zone status |
| 0x401 | TX (panel) | Light zone command |

All payload structs are defined in `src/can/can_bus.h`.

---

## Getting started

### Prerequisites

- [PlatformIO Core](https://platformio.org/install) (CLI or VS Code extension)
- Python 3.x (for PlatformIO)

### Build and flash

```bash
# Install dependencies and build
pio run

# Flash to board (adjust port if needed)
pio run --target upload

# Open serial monitor
pio device monitor
```

### Touch calibration

If touch input seems misaligned, open the **Settings** screen on the panel and tap **Calibrate Touch**. Follow the Serial monitor prompts to touch three target points. Update the resulting `TOUCH_X_MIN/MAX` and `TOUCH_Y_MIN/MAX` values in `include/config.h`.

---

## UI screens

- **Home** — temperature, humidity tiles; HVAC summary; All ON / All OFF light buttons
- **HVAC** — mode selection (Off/Heat/Cool/Fan/Auto), fan speed, setpoint +/- buttons, Apply
- **Lights** — per-zone toggle with brightness bar; All On / All Off controls
- **Settings** — firmware info, CAN diagnostics, backlight control, touch calibration launcher

---

## Configuration

All hardware pin assignments, CAN IDs, colors, and timing constants live in `include/config.h`. No changes to source files should be needed for a standard wiring.

For TFT_eSPI driver settings, the `build_flags` in `platformio.ini` pass the correct defines directly — no separate `User_Setup.h` file is required.

---

## License

MIT — see LICENSE file (not included; add your own).
