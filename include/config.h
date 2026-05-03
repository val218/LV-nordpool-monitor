// config.h — central configuration for the Nordpool monitor (JC4827W543 + LVGL).
#pragma once

#include <Arduino.h>

// ---- Board identity ----
#define BOARD_NAME "Guition JC4827W543"

// ---- Display (NV3041A 480x272 over QSPI) ----
// Pin assignments confirmed against the JC4827W543 schematic & community drivers.
// The NV3041A uses Quad-SPI for data, which is why D0..D3 are listed instead of MOSI.
#define DISP_W            480
#define DISP_H            272

#define LCD_PIN_CS        45
#define LCD_PIN_SCK       47
#define LCD_PIN_D0        21
#define LCD_PIN_D1        48
#define LCD_PIN_D2        40
#define LCD_PIN_D3        39
#define LCD_PIN_RST       -1   // tied to chip enable on this board, no GPIO needed
#define LCD_PIN_BL         1   // backlight enable + PWM

// ---- Touch (GT911 over INTERNAL I2C — Wire / I2C0) ----
// The GT911 capacitive touch controller is wired to internal traces on
// the JC4827W543 board. These pins are NOT exposed on the external
// QWIIC / extension header — they exist only on the PCB to talk to the
// touch chip. Don't try to use them for anything else.
#define I2C_TOUCH_SDA      8
#define I2C_TOUCH_SCL      4
#define I2C_FREQ_HZ        400000

// Backwards-compatibility aliases (older code referenced I2C_SDA / I2C_SCL
// when the touch and relay buses were shared).
#define I2C_SDA            I2C_TOUCH_SDA
#define I2C_SCL            I2C_TOUCH_SCL

#define TOUCH_INT          3
#define TOUCH_RST         38
#define TOUCH_I2C_ADDR    0x5D    // GT911 default; sometimes 0x14 on rev. boards

// ---- Relay bus (XL9535 over EXTERNAL I2C — Wire1 / I2C1) ----
// The external header on the JC4827W543 brings GPIO17 / GPIO18 to the
// outside world. We use them as a SECOND I2C bus so the relay board
// doesn't share wiring with the touch controller. Standard QWIIC pinout:
//   pin 1 = GND       → board GND
//   pin 2 = 3V3       → board 3V3 (signal-side power, VIN on the relay)
//   pin 3 = SDA = 17  → board SDA
//   pin 4 = SCL = 18  → board SCL
// Relay coils need their own 5V supply (not from the QWIIC connector).
#define I2C_RELAY_SDA     17
#define I2C_RELAY_SCL     18
#define I2C_RELAY_FREQ_HZ 400000

// ---- XL9535 8-channel relay board ----
// Address pins A0/A1/A2 are solder pads on the relay board. All three
// LEFT OPEN (factory default) ⇒ address 0x20. Closing pads adds 1 to the
// corresponding bit (A0=1, A1=2, A2=4), so the addresses go 0x20..0x27.
// You only need to change this if you stack multiple relay boards on the
// same bus. The board has built-in I2C pull-ups, so no external resistors
// are needed.
// All 8 outputs are mapped to relays in pin order P0..P7.
#define XL9535_I2C_ADDR   0x20
#define NUM_RELAYS         8

// On the XL9535-K8V5, an output of LOW activates the optocoupler which energises
// the relay coil. This is the same convention as classic active-low relay
// modules. Set true so a "relay ON" state gets driven LOW on the expander pin.
#define RELAY_ACTIVE_LOW  true

// ---- Data source ----
// e-cena.lv serves a small (~7 KB) HTML page with today's and tomorrow's
// prices at 15-minute resolution. We scrape it and average every 4 entries
// into an hourly value to keep the existing UI / data model.
//
// Backup source (~1.87 MB CSV with full history) — only used if the primary
// fails. Defining both lets us add fallback later.
#define ECENA_URL          "https://www.e-cena.lv/"
#define NORDPOOL_CSV_URL   "https://nordpool.didnt.work/nordpool-lv-1h.csv"
#define PRICE_REFRESH_MS (30UL * 60UL * 1000UL)
#define RELAY_EVAL_MS    (60UL * 1000UL)

// When the price evaluator decides to turn ON multiple relays in the
// same cycle, we space those switch-ons out by this many seconds. This
// prevents inrush spikes from several heating loads kicking in at once
// and gives the inverter / breaker time to recover. Turn-OFFs are NOT
// staggered — when prices go high we want every load shed immediately.
//
// Set to 0 to disable staggering entirely (instant switching).
#define RELAY_STAGGER_SEC  5

// ---- VAT ----
#define DEFAULT_VAT_PERCENT 21.0f

// ---- Time ----
#define TZ_INFO "EET-2EEST,M3.5.0/3,M10.5.0/4"
#define NTP_SERVER_1 "europe.pool.ntp.org"
#define NTP_SERVER_2 "pool.ntp.org"

// ---- Storage ----
#define PREFS_NS "npmon"

// ---- AP mode ----
#define AP_SSID "NordpoolMonitor"
#define AP_PASS "nordpool123"

// ---- Price buffer ----
#define MAX_PRICE_ENTRIES 72
