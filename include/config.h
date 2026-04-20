// config.h — central configuration for the Nordpool monitor (RA8875 build)
#pragma once

#include <Arduino.h>

// ---- Screen (landscape 800x480) ----
#define SCREEN_W 800
#define SCREEN_H 480

// ---- RA8875 SPI wiring to ESP32 ----
// Default ESP32 VSPI bus: SCK=18, MOSI=23, MISO=19. CS and RST are user choice.
#define RA8875_CS   5
#define RA8875_RST  4
#define RA8875_INT  2      // touch interrupt — optional but speeds up touch polling

// ---- Resistive touch calibration (raw ADC -> screen coords) ----
// Start with generous defaults; refine after running a once-off calibration.
// The RA8875's 10-bit touch ADC returns ~0..1023 on both axes, but the usable
// range rarely covers the full span. These values map the typical usable raw
// range onto the 800x480 screen.
#define TOUCH_X_MIN 120
#define TOUCH_X_MAX 930
#define TOUCH_Y_MIN 100
#define TOUCH_Y_MAX 930

// ---- PCF8574 I2C relay expanders (two of them, 8 channels each) ----
// ESP32 default I2C on GPIO 21 (SDA) / 22 (SCL).
// Each PCF8574 has three address-select pins (A0/A1/A2) pulled to GND or VCC.
// For the common "PCF8574" (not "PCF8574A"), addresses are 0x20..0x27.
// For PCF8574A variants, addresses are 0x38..0x3F.
#define I2C_SDA 21
#define I2C_SCL 22
#define PCF_A_ADDR 0x20     // relays 1..8
#define PCF_B_ADDR 0x21     // relays 9..16
#define NUM_RELAYS 16
// The ULN2803A is an inverting Darlington array: HIGH input → output pulls
// to GND → relay coil energises. So from the PCF8574 side, HIGH = relay ON.
// If you ever rewire without a ULN in between, flip this back to true.
#define RELAY_ACTIVE_LOW false

// ---- Data source ----
#define NORDPOOL_CSV_URL "https://nordpool.didnt.work/nordpool-lv-1h.csv"
#define PRICE_REFRESH_MS (30UL * 60UL * 1000UL)
#define RELAY_EVAL_MS    (60UL * 1000UL)

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

// ---- Double-tap ----
#define DOUBLE_TAP_MS 450

// ---- Price buffer ----
#define MAX_PRICE_ENTRIES 72
