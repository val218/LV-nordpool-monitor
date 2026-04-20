#include "ui.h"
#include "i18n.h"
#include <time.h>

// Adafruit_RA8875 doesn't provide color565() directly; this is the standard
// packing used by every 16-bit RGB display.
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ---------- Palette (dark dashboard, matches the v6 SVG mockup) ----------
static const uint16_t COL_BG       = 0x1082;   // near-black
static const uint16_t COL_CARD     = 0x2124;   // panel surface
static const uint16_t COL_CARD_LO  = 0x18C3;
static const uint16_t COL_TEXT     = 0xFFFF;
static const uint16_t COL_DIM      = 0x8C71;
static const uint16_t COL_GRID     = 0x3186;
static const uint16_t COL_GREEN    = 0x4E66;
static const uint16_t COL_YELLOW   = 0xEF40;
static const uint16_t COL_ORANGE   = 0xFCA0;
static const uint16_t COL_RED      = 0xE124;
static const uint16_t COL_NOWBAR   = 0x5EFF;   // light blue current-hour

// ---------- Layout (800 × 480 landscape) ----------
// Scaled from the 480×320 v6 design with roughly a 1.5× factor, plus a bit
// more breathing room around edges for the much bigger panel.
static const int HDR_H       = 40;
static const int TOP_ROW_Y   = HDR_H + 8;
static const int TOP_ROW_H   = 150;
static const int CHART_Y     = TOP_ROW_Y + TOP_ROW_H + 8;
static const int CHART_H     = 224;
static const int STATS_Y     = CHART_Y + CHART_H + 8;
static const int STATS_H     = SCREEN_H - STATS_Y - 4;

// RA8875 built-in font glyph size (at scale multipliers 1..4)
static const int FONT_W[4] = {  8, 16, 24, 32 };
static const int FONT_H[4] = { 16, 32, 48, 64 };

// ---------- UI ----------
UI::UI(Adafruit_RA8875& tft, PriceData& prices, RelayController& relays)
    : _tft(tft), _prices(prices), _relays(relays) {}

void UI::begin() {
    // _tft.begin()/displayOn()/GPIOX()/touchEnable() are called from main.cpp
    // — we assume the caller has already brought the controller up.
    _tft.fillScreen(COL_BG);
    _dirtyAll = true;
}

void UI::goTo(Screen s) {
    _screen = s;
    _dirtyAll = true;
    clearHits();
    drawAll();
}

// ---- Text helpers --------------------------------------------------------

int UI::textWidth(const char* s, uint8_t scale) const {
    if (scale > 3) scale = 3;
    int n = 0; while (s[n]) n++;
    return n * FONT_W[scale];
}
int UI::textHeight(uint8_t scale) const {
    if (scale > 3) scale = 3;
    return FONT_H[scale];
}

void UI::drawText(const char* s, int x, int y, TextAlign align,
                  uint16_t fg, uint16_t bg, uint8_t scale) {
    if (scale > 3) scale = 3;
    int w = textWidth(s, scale);
    int h = textHeight(scale);
    int dx = x, dy = y;
    switch (align) {
        case TA_TL: break;
        case TA_TC: dx = x - w / 2; break;
        case TA_TR: dx = x - w;     break;
        case TA_ML: dy = y - h / 2; break;
        case TA_MC: dx = x - w / 2; dy = y - h / 2; break;
        case TA_MR: dx = x - w;     dy = y - h / 2; break;
        case TA_BL: dy = y - h;     break;
        case TA_BC: dx = x - w / 2; dy = y - h;     break;
        case TA_BR: dx = x - w;     dy = y - h;     break;
    }
    _tft.textMode();
    _tft.textSetCursor(dx, dy);
    _tft.textEnlarge(scale);
    _tft.textTransparent(fg);       // transparent: underlying bg shows through
    _tft.textColor(fg, bg);         // opaque: use bg as the fill color
    // textTransparent + textColor: the library applies textColor's bg when in
    // opaque mode. Using the opaque path here so card backgrounds overwrite
    // old characters cleanly (no ghosting on changing values).
    _tft.print(s);
    _tft.graphicsMode();
}

// ---- Misc helpers --------------------------------------------------------

String UI::currentTimeString() const {
    time_t now = time(nullptr);
    if (now < 100000) return "--:--";
    struct tm t; localtime_r(&now, &t);
    char buf[8]; snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    return String(buf);
}

uint16_t UI::priceColor(float p, float mn, float mx) const {
    if (mx <= mn) return COL_GREEN;
    float t = (p - mn) / (mx - mn);
    if (t < 0) t = 0; if (t > 1) t = 1;
    if (t < 0.33f)      return COL_GREEN;
    else if (t < 0.60f) return COL_YELLOW;
    else if (t < 0.85f) return COL_ORANGE;
    else                return COL_RED;
}

void UI::formatPrice(float raw, char* buf, size_t bufLen) const {
    float p = _showVat ? raw * (1.0f + _vat / 100.0f) : raw;
    snprintf(buf, bufLen, "%.1f", p * 100.0f);
}

void UI::fillCard(int x, int y, int w, int h, uint16_t col) {
    // Plain filled rect — the RA8875 has no drawRoundRect primitive.
    _tft.fillRect(x, y, w, h, col);
}

// ---- Top-level render ----------------------------------------------------

void UI::drawAll() {
    _tft.fillScreen(COL_BG);
    drawHeader();
    switch (_screen) {
        case SCR_MAIN:       drawMain(); break;
        case SCR_RELAYS:     drawRelaysScreen(); break;
        case SCR_RELAY_EDIT: drawRelayEditScreen(); break;
    }
    _dirtyAll = false;
    _dirtyHeader = false;
}

void UI::refresh() {
    if (_dirtyAll) { drawAll(); return; }
    if (_dirtyHeader) { drawHeader(); _dirtyHeader = false; }
    if (_screen == SCR_MAIN) {
        String ts = currentTimeString();
        if (ts != _lastClock) drawHeader();
    }
}

void UI::drawHeader() {
    _tft.fillRect(0, 0, SCREEN_W, HDR_H, COL_BG);

    // Title, scale 1 = 16×32
    drawText(T(S_TITLE), 10, 6, TA_TL, COL_TEXT, COL_BG, 1);

    // Status dot
    uint16_t dot = _configMode ? COL_ORANGE : (_wifi ? COL_GREEN : COL_RED);
    int dotX = 250;
    _tft.fillCircle(dotX, HDR_H / 2, 7, dot);

    // IP / status string at scale 0 (16px tall)
    String s = _configMode ? String(T(S_CONFIG_MODE)) + " " + _ip
                           : (_wifi ? _ip : String(T(S_OFFLINE)));
    drawText(s.c_str(), dotX + 14, 12, TA_TL, COL_DIM, COL_BG, 0);

    // Right side: date + clock, both scale 1
    String clock = currentTimeString();
    int clockW = textWidth(clock.c_str(), 1);
    drawText(clock.c_str(), SCREEN_W - 10, 6, TA_TR, COL_TEXT, COL_BG, 1);

    time_t now = time(nullptr);
    if (now > 100000) {
        struct tm t; localtime_r(&now, &t);
        static const char* MON[12] = {"Jan","Feb","Mar","Apr","May","Jun",
                                       "Jul","Aug","Sep","Oct","Nov","Dec"};
        char db[16]; snprintf(db, sizeof(db), "%d %s", t.tm_mday, MON[t.tm_mon]);
        drawText(db, SCREEN_W - 10 - clockW - 20, 6, TA_TR, COL_DIM, COL_BG, 1);
    }

    _lastClock = clock;
}

// ---- Price card ---------------------------------------------------------

void UI::drawPriceCard(int x, int y, int w, int h,
                       const char* label, float valRaw, int scale) {
    fillCard(x, y, w, h, COL_CARD);

    // Label (top-left), scale 0 = 8×16
    drawText(label, x + 14, y + 10, TA_TL, COL_DIM, COL_CARD, 0);

    // Big number, centered
    uint16_t col = COL_TEXT;
    if (!isnan(valRaw)) {
        float dMin, dMax, dAvg;
        if (_prices.statsForDay(time(nullptr), dMin, dMax, dAvg)) {
            col = priceColor(valRaw, dMin, dMax);
        }
    }
    char buf[16];
    if (isnan(valRaw)) snprintf(buf, sizeof(buf), "--");
    else               formatPrice(valRaw, buf, sizeof(buf));
    drawText(buf, x + w / 2, y + h / 2 + 4, TA_MC, col, COL_CARD, scale);

    // Units (bottom-center), scale 0
    const char* unit = _showVat ? "c/kWh +VAT" : T(S_CENTS_PER_KWH);
    drawText(unit, x + w / 2, y + h - 8, TA_BC, COL_DIM, COL_CARD, 0);
}

void UI::drawMain() {
    time_t now = time(nullptr);
    const PriceEntry* prv = _prices.getPrevious(now);
    const PriceEntry* cur = _prices.getCurrent(now);
    const PriceEntry* nxt = _prices.getNext(now);

    // 1 : 2 : 1 card widths
    int pad = 8;
    int unitW = (SCREEN_W - 4 * pad) / 4;
    int sideW = unitW;
    int centerW = unitW * 2;

    int prevX   = pad;
    int centerX = prevX + sideW + pad;
    int nextX   = centerX + centerW + pad;

    // Side cards use scale 2 (24×48), center card uses scale 3 (32×64) — so
    // the current price is roughly double the size in both dimensions.
    drawPriceCard(prevX,   TOP_ROW_Y, sideW,   TOP_ROW_H, T(S_PREV),
                  prv ? prv->price_raw : NAN, 2);
    drawPriceCard(centerX, TOP_ROW_Y, centerW, TOP_ROW_H, T(S_CURRENT),
                  cur ? cur->price_raw : NAN, 3);
    drawPriceCard(nextX,   TOP_ROW_Y, sideW,   TOP_ROW_H, T(S_NEXT),
                  nxt ? nxt->price_raw : NAN, 2);

    drawChart();
    drawBottomStats();
}

// ---- Chart --------------------------------------------------------------

void UI::drawChart() {
    int x = 8, y = CHART_Y, w = SCREEN_W - 16, h = CHART_H;
    fillCard(x, y, w, h, COL_CARD);

    if (_prices.count() == 0) {
        drawText(T(S_NO_DATA), x + w / 2, y + h / 2, TA_MC,
                 COL_DIM, COL_CARD, 1);
        return;
    }

    time_t now = time(nullptr);
    struct tm tnow; localtime_r(&now, &tnow);
    tnow.tm_hour = 0; tnow.tm_min = 0; tnow.tm_sec = 0;
    time_t todayStart = mktime(&tnow);

    int first = -1, last = -1;
    for (int i = 0; i < _prices.count(); ++i) {
        time_t ts = _prices.at(i).ts_start;
        if (ts >= todayStart && ts < todayStart + 48 * 3600) {
            if (first < 0) first = i;
            last = i;
        }
    }
    if (first < 0) { first = 0; last = _prices.count() - 1; }
    int n = last - first + 1;
    if (n <= 0) return;

    float mn = _prices.at(first).price_raw, mx = mn;
    int minIdx = first, maxIdx = first;
    for (int i = first; i <= last; ++i) {
        float p = _prices.at(i).price_raw;
        if (p < mn) { mn = p; minIdx = i; }
        if (p > mx) { mx = p; maxIdx = i; }
    }
    float lo = mn < 0 ? mn : 0;
    float hi = mx * 1.05f;
    if (hi - lo < 0.01f) hi = lo + 0.01f;

    int axLabelW = 52;     // y-axis label column — font 0 is 8px wide
    int xLabelH  = 28;     // bigger font 1 for hour labels → more vertical space
    int gx = x + axLabelW, gy = y + 20;
    int gw = w - axLabelW - 16, gh = h - 24 - xLabelH;
    int yZero = gy + gh - (int)((0 - lo) / (hi - lo) * gh);
    if (yZero < gy) yZero = gy;
    if (yZero > gy + gh) yZero = gy + gh;

    // Dashed horizontal gridlines + y-axis labels
    int ticks = 4;
    for (int k = 0; k <= ticks; ++k) {
        float v = lo + (hi - lo) * k / ticks;
        int ly = gy + gh - (int)((v - lo) / (hi - lo) * gh);
        for (int dx = gx; dx < gx + gw; dx += 10) {
            _tft.drawLine(dx, ly, dx + 5, ly, COL_GRID);
        }
        char lb[8];
        snprintf(lb, sizeof(lb), "%.2f", (_showVat ? v * (1.0f + _vat / 100.0f) : v));
        drawText(lb, x + axLabelW - 4, ly, TA_MR, COL_DIM, COL_CARD, 0);
    }

    drawText(_showVat ? "EUR/kWh+VAT" : T(S_EUR_PER_KWH), x + 8, y + 4,
             TA_TL, COL_DIM, COL_CARD, 0);

    // Bars — 48 of them → ~14.5 px each with this chart width, very readable.
    int barW = gw / n;
    if (barW < 2) barW = 2;
    int totalW = barW * n;
    int startX = gx + (gw - totalW) / 2;

    int nowIdx = -1;
    for (int i = first; i <= last; ++i) {
        if (now >= _prices.at(i).ts_start && now < _prices.at(i).ts_start + 3600) {
            nowIdx = i; break;
        }
    }

    for (int i = first; i <= last; ++i) {
        float p = _prices.at(i).price_raw;
        int topY = gy + gh - (int)((p - lo) / (hi - lo) * gh);
        int bx = startX + (i - first) * barW;
        bool isNow = (i == nowIdx);
        uint16_t col = isNow ? COL_NOWBAR : priceColor(p, mn, mx);
        int by = (p >= 0) ? topY : yZero;
        int bh = (p >= 0) ? (yZero - topY) : (topY - yZero);
        if (bh < 1) bh = 1;
        _tft.fillRect(bx + 1, by, barW - 2, bh, col);

        if (isNow) {
            _tft.drawRect(bx, gy, barW, gh + 1, COL_NOWBAR);
        }
    }

    // "Now" pill above the current bar
    if (nowIdx >= 0) {
        int nx = startX + (nowIdx - first) * barW + barW / 2;
        int pw = 54, ph = 20;
        int px = nx - pw / 2;
        if (px < gx) px = gx;
        if (px + pw > gx + gw) px = gx + gw - pw;
        _tft.fillRect(px, gy - 4, pw, ph, COL_NOWBAR);
        drawText("Now", px + pw / 2, gy - 4 + ph / 2, TA_MC,
                 COL_BG, COL_NOWBAR, 0);
    }

    // Min & max callouts — clamped inside the chart area so they can't
    // collide with the x-axis time labels.
    auto callout = [&](int idx, float v, uint16_t pillCol) {
        int bx = startX + (idx - first) * barW + barW / 2;
        int topY = gy + gh - (int)((v - lo) / (hi - lo) * gh);
        char lb[8];
        snprintf(lb, sizeof(lb), "%.2f", (_showVat ? v * (1.0f + _vat / 100.0f) : v));
        int tw = textWidth(lb, 0) + 12;
        int th = 18;
        int px = bx - tw / 2;
        int py = topY - th - 6;
        if (py < gy + 4) {
            int below = topY + 6;
            if (below + th <= gy + gh - 4) py = below;
            else                           py = gy + 4;
        }
        if (px < gx) px = gx;
        if (px + tw > gx + gw) px = gx + gw - tw;
        _tft.fillRect(px, py, tw, th, pillCol);
        drawText(lb, px + tw / 2, py + th / 2, TA_MC, COL_TEXT, pillCol, 0);
        _tft.fillCircle(bx, topY, 3, pillCol);
    };
    callout(maxIdx, mx, COL_ORANGE);
    callout(minIdx, mn, COL_GREEN);

    // X-axis labels every 4h, scale 1 = 16×32
    for (int i = first; i <= last; ++i) {
        struct tm t; time_t ts = _prices.at(i).ts_start;
        localtime_r(&ts, &t);
        if (t.tm_hour % 4 == 0) {
            int bx = startX + (i - first) * barW + barW / 2;
            char hb[6]; snprintf(hb, sizeof(hb), "%02d:00", t.tm_hour);
            drawText(hb, bx, gy + gh + 4, TA_TC, COL_TEXT, COL_CARD, 0);
        }
    }

    // Axis baseline
    _tft.drawLine(gx, gy + gh, gx + gw, gy + gh, COL_DIM);
}

// ---- Bottom stats row ---------------------------------------------------

void UI::drawBottomStats() {
    int x = 8, y = STATS_Y, w = SCREEN_W - 16, h = STATS_H;

    time_t now = time(nullptr);
    float dMin = 0, dMax = 0, dAvg = 0;
    bool haveDay = _prices.statsForDay(now, dMin, dMax, dAvg);

    float gMin, gMax, gAvg;
    _prices.stats(gMin, gMax, gAvg);
    bool haveAny = _prices.count() > 0;

    int pad = 8;
    int cW = (w - 3 * pad) / 4;

    auto drawStat = [&](int idx, const char* label, const char* valStr, uint16_t valCol) {
        int cX = x + idx * (cW + pad);
        fillCard(cX, y, cW, h, COL_CARD);
        drawText(label, cX + 12, y + h / 2, TA_ML, COL_DIM, COL_CARD, 0);
        drawText(valStr, cX + cW - 12, y + h / 2, TA_MR, valCol, COL_CARD, 1);
    };

    char buf[16];
    if (haveDay) {
        formatPrice(dAvg, buf, sizeof(buf));
        drawStat(0, T(S_PRICES_TODAY), buf, COL_TEXT);
        formatPrice(dMin, buf, sizeof(buf));
        drawStat(1, T(S_MIN_PRICE), buf, COL_GREEN);
        formatPrice(dMax, buf, sizeof(buf));
        drawStat(2, T(S_MAX_PRICE), buf, COL_ORANGE);
    } else if (haveAny) {
        formatPrice(gAvg, buf, sizeof(buf));
        drawStat(0, "Avg", buf, COL_TEXT);
        formatPrice(gMin, buf, sizeof(buf));
        drawStat(1, T(S_MIN_PRICE), buf, COL_GREEN);
        formatPrice(gMax, buf, sizeof(buf));
        drawStat(2, T(S_MAX_PRICE), buf, COL_ORANGE);
    } else {
        drawStat(0, T(S_PRICES_TODAY), "--", COL_DIM);
        drawStat(1, T(S_MIN_PRICE),    "--", COL_DIM);
        drawStat(2, T(S_MAX_PRICE),    "--", COL_DIM);
    }

    time_t lu = _prices.lastUpdate();
    char ub[16];
    if (lu > 100000) {
        struct tm t; localtime_r(&lu, &t);
        snprintf(ub, sizeof(ub), "%02d:%02d", t.tm_hour, t.tm_min);
    } else {
        snprintf(ub, sizeof(ub), "--");
    }
    drawStat(3, T(S_LAST_UPDATE), ub, COL_TEXT);
}

// ---- Relay screens ------------------------------------------------------

void UI::drawRelaysScreen() {
    clearHits();
    _tft.fillScreen(COL_BG);
    drawHeader();

    // 4 × 4 grid for 16 relays. Tiles are smaller than the 8-relay version, so
    // we use smaller fonts inside — still finger-tappable on a 7" screen.
    int top = HDR_H + 10;
    int bottom = SCREEN_H - 60;
    int gridH = bottom - top;
    int cols = 4, rows = 4;
    int padX = 8, padY = 8;
    int tileW = (SCREEN_W - padX * (cols + 1)) / cols;
    int tileH = (gridH - padY * (rows + 1)) / rows;

    for (int i = 0; i < NUM_RELAYS; ++i) {
        int r = i / cols, c = i % cols;
        int tx = padX + c * (tileW + padX);
        int ty = top + padY + r * (tileH + padY);

        const RelayRule& rr = _relays.rule(i);
        uint16_t bg = rr.state ? rgb565(16, 70, 34) : COL_CARD;
        fillCard(tx, ty, tileW, tileH, bg);
        _tft.drawRect(tx, ty, tileW, tileH, rr.state ? COL_GREEN : COL_CARD_LO);

        // Name + state badge on the same line (scale 0 = 8×16)
        drawText(rr.name, tx + 10, ty + 8, TA_TL, COL_TEXT, bg, 0);
        drawText(rr.state ? T(S_ON) : T(S_OFF),
                 tx + tileW - 10, ty + 8, TA_TR,
                 rr.state ? COL_GREEN : COL_DIM, bg, 0);

        // Mode line
        const char* modeStr = "";
        switch (rr.mode) {
            case RMODE_ALWAYS_OFF: modeStr = T(S_ALWAYS_OFF); break;
            case RMODE_ALWAYS_ON:  modeStr = T(S_ALWAYS_ON);  break;
            case RMODE_ON_BELOW:   modeStr = T(S_TURN_ON_BELOW); break;
            case RMODE_OFF_ABOVE:  modeStr = T(S_TURN_OFF_ABOVE); break;
        }
        drawText(modeStr, tx + 10, ty + 30, TA_TL, COL_DIM, bg, 0);

        // Threshold (only shown for modes that use one)
        if (rr.mode == RMODE_ON_BELOW || rr.mode == RMODE_OFF_ABOVE) {
            char b[24];
            snprintf(b, sizeof(b), "%.1f", rr.threshold * 100.0f);
            drawText(b, tx + 10, ty + 48, TA_TL, COL_TEXT, bg, 1);
        }

        addHit(tx, ty, tileW, tileH, 100 + i);
    }

    int bbW = 160, bbH = 44;
    int bbX = SCREEN_W - bbW - 12, bbY = SCREEN_H - bbH - 8;
    fillCard(bbX, bbY, bbW, bbH, COL_CARD);
    drawText(T(S_BACK), bbX + bbW / 2, bbY + bbH / 2, TA_MC, COL_TEXT, COL_CARD, 1);
    addHit(bbX, bbY, bbW, bbH, 1);
}

void UI::drawRelayEditScreen() {
    clearHits();
    _tft.fillScreen(COL_BG);
    drawHeader();

    int i = _editIdx;
    if (i < 0 || i >= NUM_RELAYS) { goTo(SCR_RELAYS); return; }
    RelayRule& rr = _relays.rule(i);

    char title[48]; snprintf(title, sizeof(title), "%s %d  %s", T(S_RELAY), i + 1, rr.name);
    drawText(title, 16, HDR_H + 14, TA_TL, COL_TEXT, COL_BG, 2);

    const char* labels[4] = { T(S_ALWAYS_OFF), T(S_ALWAYS_ON),
                              T(S_TURN_ON_BELOW), T(S_TURN_OFF_ABOVE) };
    int btnY = HDR_H + 80;
    int btnH = 72;
    int gap = 12;
    int btnW = (SCREEN_W - 5 * gap) / 4;
    for (int m = 0; m < 4; ++m) {
        int bx = gap + m * (btnW + gap);
        bool active = (int)rr.mode == m;
        uint16_t bg = active ? COL_NOWBAR : COL_CARD;
        fillCard(bx, btnY, btnW, btnH, bg);
        drawText(labels[m], bx + btnW / 2, btnY + btnH / 2, TA_MC,
                 active ? COL_BG : COL_TEXT, bg, 1);
        addHit(bx, btnY, btnW, btnH, 200 + m);
    }

    int tY = btnY + btnH + 30;
    drawText(T(S_THRESHOLD), 16, tY, TA_TL, COL_DIM, COL_BG, 1);

    int valY = tY + 50;
    int valH = 100;
    int minusX = 16, minusW = 120;
    int plusX = SCREEN_W - 120 - 16, plusW = 120;

    fillCard(minusX, valY, minusW, valH, COL_CARD);
    drawText("-", minusX + minusW / 2, valY + valH / 2, TA_MC, COL_TEXT, COL_CARD, 3);
    addHit(minusX, valY, minusW, valH, 300);

    fillCard(plusX, valY, plusW, valH, COL_CARD);
    drawText("+", plusX + plusW / 2, valY + valH / 2, TA_MC, COL_TEXT, COL_CARD, 3);
    addHit(plusX, valY, plusW, valH, 301);

    char vbuf[32];
    snprintf(vbuf, sizeof(vbuf), "%.1f %s", rr.threshold * 100.0f, T(S_CENTS_PER_KWH));
    int vX = minusX + minusW + 16;
    int vW = plusX - vX - 16;
    fillCard(vX, valY, vW, valH, COL_CARD);
    drawText(vbuf, vX + vW / 2, valY + valH / 2, TA_MC, COL_TEXT, COL_CARD, 2);

    int bbW = 160, bbH = 44;
    int bbX = SCREEN_W - bbW - 12, bbY = SCREEN_H - bbH - 8;
    fillCard(bbX, bbY, bbW, bbH, COL_CARD);
    drawText(T(S_BACK), bbX + bbW / 2, bbY + bbH / 2, TA_MC, COL_TEXT, COL_CARD, 1);
    addHit(bbX, bbY, bbW, bbH, 1);
}

int UI::hitTest(int x, int y) const {
    for (int i = _hitCount - 1; i >= 0; --i) {
        const Rect& r = _hitRegions[i];
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return r.tag;
    }
    return -1;
}

void UI::onTap(int x, int y) {
    if (_screen == SCR_RELAYS || _screen == SCR_RELAY_EDIT) {
        int tag = hitTest(x, y);
        if (tag < 0) return;
        if (tag == 1) {
            if (_screen == SCR_RELAY_EDIT) goTo(SCR_RELAYS);
            else goTo(SCR_MAIN);
            return;
        }
        if (_screen == SCR_RELAYS && tag >= 100 && tag < 100 + NUM_RELAYS) {
            openRelayEdit(tag - 100);
            return;
        }
        if (_screen == SCR_RELAY_EDIT) {
            RelayRule& rr = _relays.rule(_editIdx);
            if (tag >= 200 && tag < 204) {
                rr.mode = (RelayMode)(tag - 200);
                _relays.save();
                drawRelayEditScreen();
                return;
            }
            if (tag == 300) {
                rr.threshold -= 0.005f;
                if (rr.threshold < 0) rr.threshold = 0;
                _relays.save();
                drawRelayEditScreen();
                return;
            }
            if (tag == 301) {
                rr.threshold += 0.005f;
                if (rr.threshold > 2.0f) rr.threshold = 2.0f;
                _relays.save();
                drawRelayEditScreen();
                return;
            }
        }
    }
}

void UI::onDoubleTap(int, int) {
    if (_screen == SCR_MAIN) goTo(SCR_RELAYS);
    else goTo(SCR_MAIN);
}
