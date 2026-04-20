# Nordpool Electricity Price Monitor — ESP32 + RA8875 + 7" 800×480

A standalone touchscreen monitor that pulls Latvia's Nordpool day-ahead
electricity prices and controls up to 8 relays based on configurable price
thresholds. Configured from a phone via a built-in web UI.

This is the **RA8875 / 7-inch edition**. For the smaller WT32-SC01 / 3.5-inch
variant, see the previous project revision.

## Hardware

- **ESP32 DevKit** (any flavour with the default VSPI pins exposed — WROOM,
  WROVER, etc.)
- **ER-AS-RA8875** — EastRising Arduino shield with the RA8875 display
  controller
- **ER-TFTM070-5 (RTP)** — 7-inch 800×480 TFT panel with **resistive** touch
- **16-channel relay board** (e.g. Sirius-PCB "16 relay USB timer", with
  the PIC18F2550 removed — see "Relay board modification" below)
- **Two PCF8574 I2C I/O expanders** at addresses `0x20` and `0x21`
- **Separate 5 V / 1 A power supply** for the display

### Wiring

All SPI lines go straight from ESP32 3.3 V logic to the shield's SPI header.
The RA8875's SPI inputs are 5 V-tolerant but will happily accept 3.3 V signals,
and MISO on the shield is already level-shifted down to 3.3 V. **No external
level shifter is needed.**

| ESP32 GPIO | Shield header | Notes                              |
|------------|---------------|-------------------------------------|
| 18         | SCK           | VSPI clock                          |
| 23         | MOSI          | VSPI MOSI                           |
| 19         | MISO          | VSPI MISO                           |
| 5          | CS            | RA8875 chip-select (configurable)   |
| 4          | RST           | RA8875 reset (configurable)         |
| 2          | INT           | Touch interrupt (optional)          |
| GND        | GND           | **Common ground — required**        |
| *(external)* | 5V          | **Separate 5 V / ≥1 A supply**      |

The two PCF8574s sit on the ESP32's default I2C bus, at different addresses:

| ESP32 GPIO | Both PCF8574s | Notes                     |
|------------|---------------|----------------------------|
| 21         | SDA           | shared                     |
| 22         | SCL           | shared                     |
| 3V3        | VCC           | shared                     |
| GND        | GND           | shared with relay GND     |

Set the address pins on each PCF8574:

| PCF8574 | A2 | A1 | A0 | Address | Drives relays |
|---------|----|----|----|---------|----------------|
| A       | GND | GND | GND | `0x20` | 1 to 8         |
| B       | GND | GND | VCC | `0x21` | 9 to 16        |

If your PCF8574 breakouts have DIP switches / solder jumpers for the address
pins, use those. If they're hardwired to `0x20`, one of them needs to be
re-jumpered — it's usually a tiny solder bridge or a DIP switch on the back.

Each PCF8574's 8 outputs (P0–P7) then drive 8 relay inputs on the modified
relay board. See "Relay board modification" below.

### Relay board modification (Sirius-PCB 16 relay USB timer)

The Sirius-PCB "16 relay USB timer" ships with a PIC18F2550 that implements
a USB protocol we can't control from the ESP32 without additional USB-host
hardware. The cleanest workaround is to desolder the PIC and drive the
16 relay inputs directly from two PCF8574 expanders.

**Physical steps:**

1. Desolder the PIC18F2550 from the board (or, if it's socketed, just pull
   it out).
2. Identify which PIC pins were driving the relay transistors — they'll be
   the pins that go through a resistor to the base of a transistor (or to
   a ULN2803 input if the board has one).
3. Wire each of those 16 PIC pin pads to the corresponding PCF8574 output:
   - PCF8574 A outputs `P0..P7` → relay inputs 1..8
   - PCF8574 B outputs `P0..P7` → relay inputs 9..16
4. Tie the original board's GND to the ESP32 / PCF8574 GND.

**Important — check the polarity of the relay driver stage:**

- If the board uses a ULN2803 Darlington array (HIGH input = relay ON),
  leave `RELAY_ACTIVE_LOW false` in `config.h`.
- If the board uses an inverter stage (LOW input = relay ON), or discrete
  PNP transistors pulled up, leave `RELAY_ACTIVE_LOW true` (the default).

Get this wrong and all 16 relays will be inverted from what the UI shows —
just flip the flag and reflash. No hardware damage.

The relay coils need their own 5 V or 12 V supply (depending on which
relays are fitted on your board — Sirius ships with multiple variants). The
original USB-B power path is no longer used; power the coils directly.

### Power notes

- Do not try to feed the 7-inch backlight off the ESP32's USB rail. Give the
  shield its own 5 V supply and tie grounds together.
- Some ER-AS-RA8875 revisions have a jumper / slide switch selecting between
  external 5 V and Arduino-provided 5 V. Check yours and pick **external 5 V**.
- If you see the ESP32 brown out on startup, you're almost certainly sharing
  a supply that can't handle the backlight inrush.

## Build

1. Install PlatformIO (VS Code extension is easiest).
2. Open this folder as a project.
3. Connect the ESP32 via USB.
4. `pio run -t upload`.
5. Open the serial monitor at 115200 baud.

## First run

First boot → AP mode, because there are no saved Wi-Fi credentials. The header
will show `Setup Mode 192.168.4.1`.

1. Join Wi-Fi network `NordpoolMonitor` (password `nordpool123`).
2. Browse to `http://192.168.4.1`, pick your home Wi-Fi, enter the password,
   press **Connect**.
3. The device reconnects to your router; the header updates with the new IP.
4. Reconnect your phone to your normal Wi-Fi and open that IP to configure
   language, VAT, and per-relay rules.

## Touch calibration

The RA8875's built-in resistive touch returns raw 10-bit ADC values. The
defaults in `config.h` work for most ER-TFTM070-5 panels, but if your touch
is offset or stretched, adjust these four numbers until the corners of the
screen map to the corners of the touch area:

```c
#define TOUCH_X_MIN 120
#define TOUCH_X_MAX 930
#define TOUCH_Y_MIN 100
#define TOUCH_Y_MAX 930
```

The easy way to find the right values: temporarily add `Serial.printf("raw %d %d\n", rx, ry);`
inside `handleTouch()` in `main.cpp`, touch each screen corner, and read the
raw values off the serial monitor. Plug those into `config.h` and rebuild.

## Usage

- **Main screen** — prev / current / next hour prices, then a colored 48-hour
  bar chart with the current hour highlighted in light blue, then a row of
  today's stats (avg / min / max) and last-update time.
- **Double-tap anywhere** on the main screen → relay configuration screen.
- **Tap a relay tile** → edit its mode and threshold on the touchscreen.
  Modes:
  - *Always OFF / Always ON* — ignore price
  - *ON below* — energised while current price ≤ threshold
  - *OFF above* — de-energised once current price crosses threshold
- Thresholds are always stored in EUR/kWh **without VAT**, so relay behaviour
  doesn't shift when you toggle the "Show with VAT" display setting.

## Project layout

```
nordpool-monitor/
├── platformio.ini          RA8875 + AsyncWebServer + PCF8574 + ArduinoJson
├── include/
│   ├── config.h            All tunable constants (pins, URLs, TZ, touch cal)
│   ├── i18n.h              EN / LV / RU string table
│   ├── price_data.h
│   ├── relay_controller.h
│   ├── ui.h                RA8875-based renderer
│   ├── net_manager.h
│   └── web_server.h
└── src/
    ├── main.cpp            RA8875 init + resistive touch reading
    ├── price_data.cpp
    ├── relay_controller.cpp
    ├── ui.cpp
    ├── net_manager.cpp
    └── web_server.cpp
```

## Troubleshooting

### Blank screen / `RA8875 not found — check wiring / power`

- Check MISO wiring — the RA8875 initialization reads back a register value
  over SPI to confirm the chip is alive. Bad MISO wiring = init failure.
- Check 5 V supply to the shield — the RA8875 runs off an on-shield regulator
  fed from 5 V. If the ESP32 is powered but the shield isn't, init fails.
- If `begin(RA8875_800x480)` returns `false`, the library didn't find the
  chip. Double-check CS and RST pins in `config.h`.

### Touch fires on the wrong spots

- Adjust `TOUCH_X_MIN`..`TOUCH_Y_MAX` in `config.h` (see calibration section).
- Some panels have the X axis mirrored relative to the display. If tapping
  the left edge fires on the right, swap `TOUCH_X_MIN` and `TOUCH_X_MAX` in
  `config.h` (the subtraction in `mapTouch()` handles the sign flip).

### Backlight PWM flickers

- `PWM1out(255)` = full brightness. If you see flicker at lower values it's
  usually a weak shared supply; switch to a dedicated 5 V brick.

### Text looks blocky at large sizes

- The RA8875's built-in font is a bitmap font with 1×/2×/3×/4× scaling — the
  big "current price" digits are the 4× scale. This is expected. If you want
  smoother large numbers, the Adafruit library supports GFX-style fonts via
  `setTextSize()` + GFX font files, at the cost of slower rendering. That's
  a drop-in upgrade path if you want it later.
