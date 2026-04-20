// ui.h — dashboard screen renderer for the Nordpool monitor
// Hardware: RA8875 controller on 7" 800×480 panel, resistive touch
#pragma once

#include <Arduino.h>
#include <Adafruit_RA8875.h>
#include "price_data.h"
#include "relay_controller.h"

enum Screen : uint8_t {
    SCR_MAIN = 0,
    SCR_RELAYS = 1,
    SCR_RELAY_EDIT = 2,
};

// Text alignment, mirrors what TFT_eSPI datums did so the drawing code reads
// familiarly. Only the combinations actually used are supported.
enum TextAlign : uint8_t {
    TA_TL, TA_TC, TA_TR,   // Top-Left, Top-Center, Top-Right
    TA_ML, TA_MC, TA_MR,
    TA_BL, TA_BC, TA_BR,
};

class UI {
public:
    UI(Adafruit_RA8875& tft, PriceData& prices, RelayController& relays);
    void begin();

    void drawAll();
    void refresh();

    // Input (coords already mapped to screen pixels)
    void onTap(int x, int y);
    void onDoubleTap(int x, int y);

    void setWifiConnected(bool c)       { _wifi = c; _dirtyHeader = true; }
    void setIpString(const String& ip)  { _ip = ip;  _dirtyHeader = true; }
    void setConfigMode(bool c)          { _configMode = c; _dirtyHeader = true; }

    void setShowVat(bool v)             { _showVat = v; _dirtyAll = true; }
    void setVatPercent(float v)         { _vat = v; _dirtyAll = true; }
    bool showVat() const                { return _showVat; }
    float vatPercent() const            { return _vat; }

    Screen current() const              { return _screen; }
    void goTo(Screen s);
    void openRelayEdit(int idx)         { _editIdx = idx; goTo(SCR_RELAY_EDIT); }

private:
    Adafruit_RA8875& _tft;
    PriceData&       _prices;
    RelayController& _relays;

    Screen _screen = SCR_MAIN;
    bool _wifi = false;
    bool _configMode = false;
    bool _showVat = true;
    float _vat = 21.0f;
    String _ip = "";

    bool _dirtyAll = true;
    bool _dirtyHeader = true;

    int _editIdx = 0;
    String _lastClock;

    // ---- drawing ----
    void drawHeader();
    void drawMain();
    void drawPriceCard(int x, int y, int w, int h,
                       const char* label, float valRaw, int scale);
    void drawChart();
    void drawBottomStats();
    void drawRelaysScreen();
    void drawRelayEditScreen();

    // ---- RA8875 helpers ----
    // RA8875 lacks drawRoundRect; use filled rect + 4 corner pixels erased.
    // Simpler: just plain rects. Corners are "rounded" visually by our dark
    // background swallowing the card edge — looks clean enough.
    void fillCard(int x, int y, int w, int h, uint16_t col);

    // Minimal text drawing wrappers: set color + scale, then print at an
    // (x,y) aligned to the given datum.
    // Scale: 0 → 8×16, 1 → 16×32, 2 → 24×48, 3 → 32×64 (RA8875 built-in font).
    void drawText(const char* s, int x, int y, TextAlign align,
                  uint16_t fg, uint16_t bg, uint8_t scale);
    int  textWidth(const char* s, uint8_t scale) const;
    int  textHeight(uint8_t scale) const;

    uint16_t priceColor(float priceRaw, float mn, float mx) const;
    void     formatPrice(float raw, char* buf, size_t bufLen) const;
    String   currentTimeString() const;

    // Hit testing
    struct Rect { int x, y, w, h; int tag; };
    Rect _hitRegions[32];
    int  _hitCount = 0;
    void clearHits() { _hitCount = 0; }
    void addHit(int x, int y, int w, int h, int tag) {
        if (_hitCount < (int)(sizeof(_hitRegions) / sizeof(_hitRegions[0]))) {
            _hitRegions[_hitCount++] = { x, y, w, h, tag };
        }
    }
    int hitTest(int x, int y) const;
};
