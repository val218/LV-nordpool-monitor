// main.cpp — Nordpool monitor for Guition JC4827W543 (ESP32-S3 + LVGL).

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#include "config.h"
#include "i18n.h"
#include "price_data.h"
#include "relay_controller.h"
#include "ui.h"
#include "net_manager.h"
#include "web_server.h"
#include "gt911.h"

// ---- Display: Arduino_GFX driver chain for the JC4827W543's NV3041A panel.
// The board uses a quad-SPI bus with 4 data lines; Arduino_GFX exposes that
// via Arduino_ESP32QSPI which the NV3041A class then drives.
// Arduino_NV3041A in 1.4.x has a fixed 480×272 resolution baked into the
// class — no width/height parameters in the constructor.
static Arduino_DataBus* bus =
    new Arduino_ESP32QSPI(LCD_PIN_CS, LCD_PIN_SCK,
                          LCD_PIN_D0, LCD_PIN_D1,
                          LCD_PIN_D2, LCD_PIN_D3);
static Arduino_GFX* gfx =
    new Arduino_NV3041A(bus, GFX_NOT_DEFINED /*rst*/, 0 /*rotation*/, true /*IPS*/);

// ---- LVGL display buffer (RGB565) — one full screen line × N rows.
// Rough math: 480 * 60 * 2 = 56 KB. Allocated from PSRAM since regular RAM is
// precious. Two buffers for double-buffered flushing.
static constexpr int LV_BUF_LINES = 60;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* lv_buf_a = nullptr;
static lv_color_t* lv_buf_b = nullptr;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

// ---- Globals ----
Lang             g_lang = LANG_EN;
PriceData        g_prices;
RelayController  g_relays;
NetManager       g_net;
UI               g_ui(g_prices, g_relays);
WebUI            g_web(g_net, g_relays, g_ui);
GT911            g_touch(TOUCH_INT, TOUCH_RST, TOUCH_I2C_ADDR);

// ---- LVGL flush callback: hands a rectangle of pixels to Arduino_GFX.
static void lv_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area,
                        lv_color_t* color_p) {
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;

    gfx->draw16bitRGBBitmap(area->x1, area->y1,
                             (uint16_t*)color_p, w, h);
    lv_disp_flush_ready(drv);
}

// ---- LVGL touch read callback.
static int16_t s_lastX = 0, s_lastY = 0;
static bool    s_lastDown = false;

static void lv_touch_read_cb(lv_indev_drv_t*, lv_indev_data_t* data) {
    int16_t x, y;
    if (g_touch.read(x, y)) {
        // Clamp to display bounds — GT911 occasionally returns coords slightly
        // outside the active area near the bezel.
        if (x < 0) x = 0; if (x >= DISP_W) x = DISP_W - 1;
        if (y < 0) y = 0; if (y >= DISP_H) y = DISP_H - 1;
        s_lastX = x; s_lastY = y; s_lastDown = true;
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->point.x = s_lastX;
        data->point.y = s_lastY;
        data->state = LV_INDEV_STATE_REL;
        s_lastDown = false;
    }
}

// ---- App-level helpers ----

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

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n[APP] Nordpool Monitor on %s starting\n", BOARD_NAME);

    // Time zone for Latvia (EET/EEST)
    setenv("TZ", TZ_INFO, 1);
    tzset();

    // I2C bus shared between GT911 touch and XL9535 relays.
    Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ_HZ);

    // Backlight on
    pinMode(LCD_PIN_BL, OUTPUT);
    digitalWrite(LCD_PIN_BL, HIGH);

    // Bring up the display
    if (!gfx->begin()) {
        Serial.println("[APP] gfx->begin() failed");
        while (1) delay(1000);
    }
    gfx->fillScreen(0x0000);

    // ---- LVGL ----
    lv_init();

    // Allocate draw buffers from PSRAM. ps_malloc returns NULL if PSRAM
    // isn't available — fall back to internal RAM.
    size_t bufBytes = DISP_W * LV_BUF_LINES * sizeof(lv_color_t);
    lv_buf_a = (lv_color_t*)ps_malloc(bufBytes);
    lv_buf_b = (lv_color_t*)ps_malloc(bufBytes);
    if (!lv_buf_a || !lv_buf_b) {
        Serial.println("[APP] PSRAM alloc failed — falling back to heap");
        lv_buf_a = (lv_color_t*)malloc(bufBytes);
        lv_buf_b = (lv_color_t*)malloc(bufBytes);
    }
    lv_disp_draw_buf_init(&draw_buf, lv_buf_a, lv_buf_b, DISP_W * LV_BUF_LINES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = DISP_W;
    disp_drv.ver_res  = DISP_H;
    disp_drv.flush_cb = lv_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lv_touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    // Touch chip — initialize after display so its INT pin is available.
    if (!g_touch.begin(Wire)) {
        Serial.println("[APP] GT911 not detected (touch will be unavailable)");
    } else {
        Serial.println("[APP] GT911 ready");
    }

    // Build UI now that LVGL is alive.
    g_ui.begin();
    loadSettings();

    // Relay board
    if (!g_relays.begin(XL9535_I2C_ADDR)) {
        Serial.printf("[APP] XL9535 not found at 0x%02X\n", XL9535_I2C_ADDR);
    }
    g_relays.load();

    // Wi-Fi & web server
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
    }
}

// ---- Loop timing ----
static unsigned long s_lastPriceFetch = 0;
static unsigned long s_lastRelayEval  = 0;
static unsigned long s_lastUiRefresh  = 0;

void loop() {
    g_net.loop();

    // Connection state changes ripple into the header.
    static bool lastConn = false, lastAp = false;
    bool conn = g_net.isConnected();
    bool apm  = g_net.isApMode();
    if (conn != lastConn || apm != lastAp) {
        lastConn = conn; lastAp = apm;
        g_ui.setWifiConnected(conn);
        g_ui.setConfigMode(apm);
        g_ui.setIpString(g_net.ipString());
    }

    // Drive LVGL — must be called frequently. lv_timer_handler() returns the
    // ms until the next required call; we don't bother sleeping that long
    // because Wi-Fi & price fetching also need attention.
    lv_timer_handler();

    unsigned long now = millis();
    if (now - s_lastPriceFetch > PRICE_REFRESH_MS || s_lastPriceFetch == 0) {
        if (g_net.isConnected()) {
            if (tryFetchPrices()) {
                // Trigger an immediate UI refresh once new data arrives.
                s_lastUiRefresh = 0;
            }
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

    delay(5);
}
