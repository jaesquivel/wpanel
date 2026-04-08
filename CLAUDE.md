# wpanel — Claude Code Guide

Wall-mounted home control panel firmware for ESP32-WROOM-32E.
PlatformIO project, Arduino framework, C++17.

---

## Build commands

```bash
# Build (downloads libs on first run)
pio run

# Build + flash (auto-detects port; override with --upload-port /dev/ttyUSB0)
pio run --target upload

# Serial monitor (115200 baud, exception decoder + colorize filters)
pio device monitor

# Clean build artifacts
pio run --target clean
```

No tests exist yet. There is no host-side build — all code runs on-device.

---

## Project layout

```
include/
  config.h              ALL pin defs, CAN IDs, RGB565 color palette, timing constants
src/
  main.cpp              setup() / loop() — wires the four subsystem managers
  display/
    display.h/.cpp      DisplayManager — TFT init, LEDC backlight, double-buffered sprite
  touch/
    touch.h/.cpp        TouchManager — XPT2046, calibration, PRESS/HOLD/RELEASE events
  can/
    can_bus.h/.cpp      CANBusManager — TWAI driver, RX dispatch, TX helpers
  ui/
    ui.h/.cpp           UIManager — 4 screens + nav bar, dirty-flag redraws
platformio.ini          Board, upload speed, TFT_eSPI build_flags, lib_deps
```

---

## Architecture rules

**config.h is the single source of truth.** All pin numbers, CAN IDs, colors, and
timing values live there. Never hard-code these values in .cpp files.

**DisplayManager is always double-buffered.** All drawing goes into `_screenBuf`
(a `TFT_eSprite`). Call `_display.pushBuffer()` once per frame — never call
`_tft` drawing methods directly from outside `DisplayManager`.

**Touch events are routed through UIManager.** `TouchManager::update()` returns a
`TouchEvent` each loop tick. The nav bar gets first chance; then the active screen
handler. Use `TouchManager::hitTest()` for all hit detection — do not invent
bounding-box checks inline.

**CAN state is owned by CANBusManager.** UI reads `_can.state()` (const ref) and
writes optimistic updates via `_can.stateMut()` immediately after sending a
command. Do not cache CAN state in the UI layer.

**Dirty-flag rendering.** Set `_dirty = true` whenever state changes; the UI tick
redraws at most once per `UI_UPDATE_INTERVAL_MS` (50 ms / ~20 FPS).

**No blocking in loop().** `fadeTo()` is the one intentional exception (backlight
transitions). Everywhere else, prefer non-blocking patterns and `yield()`.

---

## Hardware constraints

- **DRAM is tight.** The 320×240×2-byte sprite is ~150 KB. Avoid large stack
  allocations; prefer static or heap-allocated buffers.
- **GPIO 35 and 36 are input-only** — no pull-ups, no output. Touch IRQ (36) and
  CAN RX (35) are wired there deliberately.
- **SPI bus is shared** between TFT (CS=15) and touch (CS=33). TFT_eSPI manages
  its own CS; XPT2046_Touchscreen manages its CS. Never manually assert both at once.
- **LEDC channel 0** is reserved for the backlight (GPIO 32). Don't reuse it.
- **TWAI (CAN) is the ESP32 hardware peripheral** — only one instance, fixed to
  GPIO 5 (TX) and GPIO 35 (RX).

---

## CAN bus protocol

500 kbps, standard 11-bit IDs. Payload structs are in `src/can/can_bus.h`,
all `__attribute__((packed))`, little-endian.

| CAN ID | Direction | Struct            | Notes                              |
|--------|-----------|-------------------|------------------------------------|
| 0x100  | TX        | MsgPanelStatus    | Heartbeat every 2 s                |
| 0x101  | TX        | MsgPanelCommand   | Light / relay on/off/toggle/level  |
| 0x200  | RX        | MsgSensorTemp     | tempC_x10 (int16, e.g. 235=23.5°C) |
| 0x201  | RX        | MsgSensorHumidity | humidityPct_x10                    |
| 0x300  | RX        | MsgHvacStatus     | mode, fanSpeed, setpoint/actual    |
| 0x301  | TX        | MsgHvacStatus     | Reuses status struct for commands  |
| 0x400  | RX        | MsgLightStatus    | Per-zone state + brightness        |
| 0x401  | TX        | MsgPanelCommand   | Light zone command                 |

---

## UI layout reference

Screen: 320 × 240, landscape (rotation=1).

```
y=0  ┌─────────────────────────────────────────┐
     │  Status bar (STATUS_BAR_HEIGHT = 20 px)  │
y=22 ├─────────────────────────────────────────┤
     │                                         │
     │  Content area (UI_CONTENT_Y to y=210)   │
     │                                         │
y=210├─────────────────────────────────────────┤
     │  Nav bar (30 px, 4 × 80 px buttons)     │
y=240└─────────────────────────────────────────┘
```

Nav bar buttons (left to right): Home (0), HVAC (1), Lights (2), Settings (3).
Each is 80 px wide (`NAV_BTN_W = SCREEN_WIDTH / 4`).

---

## Color palette (RGB565, from config.h)

| Constant              | Use                          |
|-----------------------|------------------------------|
| COLOR_BACKGROUND      | Screen fill                  |
| COLOR_PANEL_BG        | Cards, nav bar, status bar   |
| COLOR_ACCENT          | Active state, teal highlight |
| COLOR_TEXT_PRIMARY    | White body text              |
| COLOR_TEXT_SECONDARY  | Labels, inactive items       |
| COLOR_TEXT_DIM        | Metadata, CAN stats          |
| COLOR_SUCCESS         | ON state, good status        |
| COLOR_WARNING         | Caution indicator            |
| COLOR_ERROR           | Error / BUS OFF              |
| COLOR_BUTTON_NORMAL   | Idle button background       |
| COLOR_BUTTON_ON/OFF   | Toggle button states         |
| COLOR_DIVIDER         | Horizontal rule, borders     |

---

## Adding a new screen

1. Add an entry to `ScreenId` enum in `src/ui/ui.h` (before `COUNT`).
2. Add `_drawXxxScreen()` and `_handleTouchXxx()` private method declarations.
3. Add cases in `UIManager::_drawFrame()` and `UIManager::update()` switch blocks.
4. Add a label to the `labels[]` array in `_drawNavBar()`.
5. Implement the draw and touch methods in `src/ui/ui.cpp`.

---

## Adding a new CAN message

1. Define a packed struct in `src/can/can_bus.h`.
2. Add the CAN ID `#define` in `include/config.h`.
3. For RX: add a field to `SystemState`, add a case in `_processMessage()`, and
   implement a `_handleXxx()` method in `can_bus.cpp`.
4. For TX: add a `sendXxx()` public method in `CANBusManager`.

---

## Touch calibration

Raw XPT2046 ADC range is configured via `TOUCH_X_MIN/MAX` and `TOUCH_Y_MIN/MAX`
in `config.h`. If touch is misaligned, run **Settings → Calibrate Touch** and
follow Serial monitor prompts, then update those constants.

`TouchManager::runCalibration()` is a blocking routine — only call it from the
Settings screen handler, never from an ISR or background task.