// main.cpp — Nordpool electricity price monitor for ESP32 + RA8875 + 7" panel

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_RA8875.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "i18n.h"
#include "price_data.h"
#include "relay_controller.h"
#include "ui.h"
#include "net_manager.h"
#include "web_server.h"

// ---- Globals ----
Lang             g_lang = LANG_EN;
Adafruit_RA8875  tft(RA8875_CS, RA8875_RST);
PriceData        g_prices;
RelayController  g_relays;
NetManager       g_net;
UI               g_ui(tft, g_prices, g_relays);
WebUI            g_web(g_net, g_relays, g_ui);

// ---- Timing ----
static unsigned long s_lastPriceFetch = 0;
static unsigned long s_lastRelayEval  = 0;
static unsigned long s_lastUiRefresh  = 0;

// ---- Touch state ----
static bool          s_touchDown = false;
static unsigned long s_lastTapMs = 0;
static int           s_downX = 0, s_downY = 0;

static void loadSettings() {
    Preferences p;
    p.begin(PREFS_NS, true);
    g_lang = (Lang)p.getUChar("lang", LANG_EN);
    float vat = p.getFloat("vat", DEFAULT_VAT_PERCENT);
    bool showVat = p.getBool("showvat", true);
    p.end();
    g_ui.setVatPercent(vat);
    g_ui.setShowVat(showVat);
}

static bool tryFetchPrices() {
    if (!g_net.isConnected()) return false;
    String csv;
    if (!g_net.fetchPricesCsv(csv)) return false;
    int n = g_prices.parseCsv(csv);
    Serial.printf("[APP] parsed %d price entries\n", n);
    return n > 0;
}

// Map RA8875 touch ADC output to screen pixels. The RA8875 returns 10-bit
// values in the ~0..1023 range for each axis, but the usable window is
// typically narrower — see TOUCH_X_MIN..TOUCH_Y_MAX in config.h.
static void mapTouch(uint16_t rx, uint16_t ry, int& sx, int& sy) {
    long x = (long)(rx - TOUCH_X_MIN) * SCREEN_W / (TOUCH_X_MAX - TOUCH_X_MIN);
    long y = (long)(ry - TOUCH_Y_MIN) * SCREEN_H / (TOUCH_Y_MAX - TOUCH_Y_MIN);
    if (x < 0) x = 0; if (x >= SCREEN_W) x = SCREEN_W - 1;
    if (y < 0) y = 0; if (y >= SCREEN_H) y = SCREEN_H - 1;
    sx = (int)x; sy = (int)y;
}

static void handleTouch() {
    // Use the INT pin when touch fires; fall back to polling touched() if not
    // wired. digitalRead on a pulled-up INT is LOW when touch is active.
    bool touched = (digitalRead(RA8875_INT) == LOW) && tft.touched();
    if (!touched) {
        if (s_touchDown) {
            s_touchDown = false;
            unsigned long now = millis();
            if (now - s_lastTapMs < DOUBLE_TAP_MS) {
                g_ui.onDoubleTap(s_downX, s_downY);
                s_lastTapMs = 0;
            } else {
                s_lastTapMs = now;
                g_ui.onTap(s_downX, s_downY);
            }
        }
        return;
    }
    if (!s_touchDown) {
        uint16_t rx, ry;
        tft.touchRead(&rx, &ry);
        mapTouch(rx, ry, s_downX, s_downY);
        s_touchDown = true;
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[APP] Nordpool Monitor (RA8875) starting");

    // Timezone
    setenv("TZ", TZ_INFO, 1);
    tzset();

    // I2C for PCF8574 relay expander
    Wire.begin(I2C_SDA, I2C_SCL, 400000);

    // RA8875 on default VSPI (SCK=18, MISO=19, MOSI=23)
    SPI.begin();
    if (!tft.begin(RA8875_800x480)) {
        Serial.println("[APP] RA8875 not found — check wiring / power");
        // No point continuing; halt.
        while (1) delay(1000);
    }
    Serial.println("[APP] RA8875 ready");

    tft.displayOn(true);
    tft.GPIOX(true);                // enable TFT backlight power pin
    tft.PWM1config(true, RA8875_PWM_CLK_DIV1024);  // backlight PWM
    tft.PWM1out(255);               // full brightness

    // Enable resistive touch (uses RA8875 built-in controller)
    tft.touchEnable(true);
    pinMode(RA8875_INT, INPUT_PULLUP);

    g_ui.begin();
    loadSettings();
    g_ui.drawAll();

    if (!g_relays.begin(PCF_A_ADDR, PCF_B_ADDR)) {
        Serial.printf("[APP] One or both PCF8574s missing (expected 0x%02X and 0x%02X)\n",
                      PCF_A_ADDR, PCF_B_ADDR);
    }
    g_relays.load();

    g_net.begin();
    g_ui.setWifiConnected(g_net.isConnected());
    g_ui.setConfigMode(g_net.isApMode());
    g_ui.setIpString(g_net.ipString());
    g_web.begin();

    configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
    WiFi.scanNetworks(true);

    if (g_net.isConnected()) {
        delay(500);
        tryFetchPrices();
        g_ui.drawAll();
    }
}

void loop() {
    g_net.loop();

    static bool lastConn = false, lastAp = false;
    bool conn = g_net.isConnected();
    bool apm  = g_net.isApMode();
    if (conn != lastConn || apm != lastAp) {
        lastConn = conn; lastAp = apm;
        g_ui.setWifiConnected(conn);
        g_ui.setConfigMode(apm);
        g_ui.setIpString(g_net.ipString());
    }

    handleTouch();

    unsigned long now = millis();
    if (now - s_lastPriceFetch > PRICE_REFRESH_MS || s_lastPriceFetch == 0) {
        if (g_net.isConnected()) {
            if (tryFetchPrices()) g_ui.drawAll();
            s_lastPriceFetch = now;
        }
    }

    if (now - s_lastRelayEval > RELAY_EVAL_MS) {
        time_t t = time(nullptr);
        const PriceEntry* cur = g_prices.getCurrent(t);
        g_relays.evaluate(cur ? cur->price_raw : -1.0f);
        s_lastRelayEval = now;
    }

    if (now - s_lastUiRefresh > 1000) {
        g_ui.refresh();
        s_lastUiRefresh = now;
    }

    delay(10);
}
