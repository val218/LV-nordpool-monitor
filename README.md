# Nordpool Electricity Price Monitor — Guition JC4827W543 + LVGL

A polished standalone touchscreen monitor that pulls Latvia's Nordpool
day-ahead electricity prices and controls 8 relays based on configurable
price thresholds. Configured via a built-in web UI from a phone.

This is the **JC4827W543 / LVGL edition**. Earlier editions targeted the
WT32-SC01 (3.5") and the ER-AS-RA8875 + 7" panel. The Sirius-PCB 16-relay
hack is not used in this build — a clean off-the-shelf XL9535 I2C relay
board replaces it.

## Hardware

- **Guition JC4827W543** — ESP32-S3-WROOM-1 N4R8 (4 MB flash, 8 MB PSRAM)
  with a 4.3" 480×272 IPS NV3041A panel and capacitive GT911 touch.
- **XL9535-K8V5 8-channel I2C relay board** — 8 SPDT relays driven by the
  XL9535 16-bit I2C IO expander at address 0x20.
- **External 5 V / 1 A supply** for the relay-coil rail (the JC4827W543 is
  powered separately from its USB-C port).

### Wiring

The JC4827W543's QWIIC connector exposes the same I2C bus that drives the
GT911 touch chip on the back of the display. We share that bus with the
XL9535 — no extra GPIOs needed.

| JC4827W543 (QWIIC) | XL9535-K8V5 |
|--------------------|-------------|
| 3V3                | VIN (logic) |
| GND                | GND (logic) |
| GPIO 8 (SDA)       | SDA         |
| GPIO 4 (SCL)       | SCL         |

Relay-coil side:

| External 5 V supply | XL9535-K8V5 |
|---------------------|-------------|
| +5 V                | VCC (relay) |
| GND                 | GND (relay) |

The two grounds (logic and relay) are bridged by a removable jumper on
most XL9535-K8V5 boards. Keep it in place unless you specifically need
isolated grounds.

### Address selection

XL9535-K8V5 ships with A0/A1/A2 wired to GND ⇒ I2C address 0x20. If you
chain multiple boards, change one of the address pins per board so each
shows up uniquely (0x20…0x27).

## Build

1. Install PlatformIO (the VS Code extension is easiest).
2. Open this folder as a project.
3. Connect the JC4827W543 via USB-C.
4. `pio run -e LV-nordpool-monitor -t upload`.
5. Open the serial monitor at 115200 baud.

The first build pulls a lot of dependencies (LVGL is ~5 MB of source) so
expect 2–5 minutes the first time. Subsequent builds are seconds.

## First run

First boot → AP mode, because there are no saved Wi-Fi credentials.

1. Join Wi-Fi network `NordpoolMonitor` (password `nordpool123`).
2. Browse to `http://192.168.4.1`, pick your home Wi-Fi, enter the password,
   tap **Connect**.
3. The device reconnects to your router; the header updates with the new IP.
4. Reconnect your phone to your home Wi-Fi and open that IP to configure
   language, VAT, and per-relay rules.

## UI

- **Main screen** — header with title / status / date / clock; three price
  cards (previous, current, next); a 48-hour bar chart; four mini stat
  cards (today's average, min, max, and last-update time).
- **Tap anywhere** on the main screen → relay configuration grid.
- **Tap a relay tile** → edit its mode and threshold.
- Modes:
  - *Always OFF / Always ON* — ignore price
  - *ON below* — energised while current price ≤ threshold
  - *OFF above* — de-energised once current price crosses threshold

Thresholds are stored in EUR/kWh **without** VAT, so relay behaviour
doesn't shift when you toggle the "Show with VAT" display setting.

## Project layout

```
LV-nordpool-monitor/
├── platformio.ini
├── scripts/
│   └── merge_firmware.py   custom build_merged target
├── include/
│   ├── config.h            board pinout, I2C addrs, URLs, TZ
│   ├── lv_conf.h           LVGL v8.4 configuration
│   ├── i18n.h              EN / LV / RU string table
│   ├── price_data.h
│   ├── relay_controller.h
│   ├── ui.h
│   ├── net_manager.h
│   ├── web_server.h
│   ├── gt911.h             touch driver
│   └── xl9535.h            relay-board driver
└── src/
    ├── main.cpp            display init, LVGL init, main loop
    ├── price_data.cpp
    ├── relay_controller.cpp
    ├── ui.cpp              LVGL widget tree (3 screens)
    ├── net_manager.cpp
    ├── web_server.cpp
    ├── gt911.cpp
    └── xl9535.cpp
```

## Troubleshooting

### Blank screen after flash

- Check the backlight pin (`LCD_PIN_BL` = GPIO 1) is defined and being
  driven high during `setup()`.
- Confirm PSRAM is enabled (`board_build.arduino.memory_type = qio_opi`)
  — without it the LVGL draw buffers fall back to internal RAM and may
  not be allocated.
- Open the serial monitor; if you see "gfx->begin() failed", the
  Arduino_GFX driver couldn't initialise the NV3041A. Verify you're on
  Arduino_GFX 1.6.1 or later.

### Touch unresponsive

- Serial should print "GT911 ready". If not, the chip didn't ACK on the
  expected I2C address. Some board batches use 0x14 instead of 0x5D —
  the driver tries both, but if both fail you have a wiring issue or a
  damaged GT911.
- Touch is read every loop iteration; if the UI feels laggy, lower the
  `delay(5)` at the bottom of `loop()` to `delay(1)`.

### Relays don't click

- Confirm the XL9535 ACKs at 0x20: serial should NOT print "XL9535 not
  found at 0x20".
- Check the relay-coil 5 V supply is connected and providing enough
  current — under-powered XL9535 boards have dim LEDs and chattering
  relays.
- The `RELAY_ACTIVE_LOW true` setting matches the XL9535-K8V5. If you
  later wire the expander to a different relay module that needs
  active-high inputs, flip it in `config.h`.

### Bar chart is single-color

- LVGL's `lv_chart` widget paints all bars in the series color. Per-bar
  coloring (price tiers green/yellow/orange/red) requires a custom draw
  callback — left out of v1 to keep the code compact. If you want it,
  see the LVGL docs on `LV_EVENT_DRAW_PART_BEGIN` for chart items.
