#include "ui.h"
#include "i18n.h"
#include <time.h>

// LVGL color helpers
static lv_color_t COL_BG;
static lv_color_t COL_CARD;
static lv_color_t COL_TEXT;
static lv_color_t COL_DIM;
static lv_color_t COL_GREEN;
static lv_color_t COL_YELLOW;
static lv_color_t COL_ORANGE;
static lv_color_t COL_RED;
static lv_color_t COL_NOWBAR;

static void initColors() {
    COL_BG     = lv_color_hex(0x101218);
    COL_CARD   = lv_color_hex(0x1E2229);
    COL_TEXT   = lv_color_hex(0xFFFFFF);
    COL_DIM    = lv_color_hex(0x8A8F99);
    COL_GREEN  = lv_color_hex(0x4CC54E);
    COL_YELLOW = lv_color_hex(0xEDD400);
    COL_ORANGE = lv_color_hex(0xFD9728);
    COL_RED    = lv_color_hex(0xE34B4B);
    COL_NOWBAR = lv_color_hex(0x55C8FF);
}

// ---- Helpers --------------------------------------------------------

UI::UI(PriceData& prices, RelayController& relays)
    : _prices(prices), _relays(relays) {}

String UI::currentTimeString() const {
    time_t now = time(nullptr);
    if (now < 100000) return "--:--";
    struct tm t; localtime_r(&now, &t);
    char buf[8]; snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    return String(buf);
}

String UI::dateString() const {
    time_t now = time(nullptr);
    if (now < 100000) return "";
    struct tm t; localtime_r(&now, &t);
    static const char* MON[12] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
    char buf[12]; snprintf(buf, sizeof(buf), "%d %s", t.tm_mday, MON[t.tm_mon]);
    return String(buf);
}

String UI::formatPrice(float raw) const {
    if (isnan(raw)) return "--";
    float p = _showVat ? raw * (1.0f + _vat / 100.0f) : raw;
    char b[16]; snprintf(b, sizeof(b), "%.1f", p * 100.0f);
    return String(b);
}

String UI::unitsLabel() const {
    return _showVat ? "c/kWh+VAT" : "c/kWh";
}

lv_color_t UI::colorForPrice(float p, float mn, float mx) const {
    if (mx <= mn) return COL_GREEN;
    float t = (p - mn) / (mx - mn);
    if (t < 0) t = 0; if (t > 1) t = 1;
    if (t < 0.33f)      return COL_GREEN;
    else if (t < 0.60f) return COL_YELLOW;
    else if (t < 0.85f) return COL_ORANGE;
    else                return COL_RED;
}

// Convenience: a dark "card" container with rounded corners, no border.
static lv_obj_t* makeCard(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, COL_CARD, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_radius(c, 6, 0);
    lv_obj_set_style_pad_all(c, 4, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

static lv_obj_t* makeLabel(lv_obj_t* parent, const char* text,
                           lv_color_t col, const lv_font_t* font) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_set_style_text_font(l, font, 0);
    return l;
}

// ---- Build & lifecycle ----------------------------------------------

void UI::begin() {
    initColors();

    // Tell LVGL's default theme to use our dark background.
    lv_disp_t* disp = lv_disp_get_default();
    lv_theme_t* th = lv_theme_default_init(
        disp, lv_palette_main(LV_PALETTE_BLUE),
              lv_palette_main(LV_PALETTE_LIGHT_BLUE),
              true,                              // dark
              LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, th);

    buildMain();
    buildRelays();
    buildEdit();
    openMain();
}

// ---------- MAIN SCREEN ----------------------------------------------
//   y=0   : header (28 px)
//   y=30  : price cards row (74 px)
//   y=108 : chart (100 px)
//   y=212 : footer stats (56 px)
//   total = 268 / 272

void UI::buildMain() {
    _scrMain = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrMain, COL_BG, 0);
    lv_obj_set_style_bg_opa(_scrMain, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_scrMain, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_scrMain, 0, 0);

    // Tap anywhere outside the chart on the main screen → relay screen.
    // We attach a click handler at the screen level; the chart absorbs taps,
    // so it acts as a natural exclusion zone.
    lv_obj_add_event_cb(_scrMain, onMainTouched, LV_EVENT_CLICKED, this);

    // ---- Header ----
    _hdrTitle  = makeLabel(_scrMain, T(S_TITLE), COL_TEXT, &lv_font_montserrat_14);
    lv_obj_align(_hdrTitle, LV_ALIGN_TOP_LEFT, 6, 6);

    _hdrStatus = lv_obj_create(_scrMain);
    lv_obj_set_size(_hdrStatus, 8, 8);
    lv_obj_set_style_radius(_hdrStatus, 4, 0);
    lv_obj_set_style_bg_color(_hdrStatus, COL_RED, 0);
    lv_obj_set_style_bg_opa(_hdrStatus, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_hdrStatus, 0, 0);
    lv_obj_align(_hdrStatus, LV_ALIGN_TOP_LEFT, 130, 12);
    lv_obj_clear_flag(_hdrStatus, LV_OBJ_FLAG_SCROLLABLE);

    _hdrIp = makeLabel(_scrMain, "", COL_DIM, &lv_font_montserrat_12);
    lv_obj_align(_hdrIp, LV_ALIGN_TOP_LEFT, 144, 9);

    _hdrClock = makeLabel(_scrMain, "--:--", COL_TEXT, &lv_font_montserrat_14);
    lv_obj_align(_hdrClock, LV_ALIGN_TOP_RIGHT, -6, 6);

    _hdrDate = makeLabel(_scrMain, "", COL_DIM, &lv_font_montserrat_12);
    lv_obj_align(_hdrDate, LV_ALIGN_TOP_RIGHT, -54, 9);

    // ---- Price cards row (1:2:1) ----
    int rowY = 30, rowH = 74;
    int padX = 6;
    int unitW = (DISP_W - 4 * padX) / 4;
    int sideW = unitW;
    int curW  = unitW * 2;

    int prevX = padX;
    int curX  = prevX + sideW + padX;
    int nextX = curX + curW + padX;

    auto buildCard = [&](lv_obj_t** card, lv_obj_t** valLbl,
                         int x, int y, int w, int h,
                         const char* title, const lv_font_t* valFont) {
        *card = makeCard(_scrMain, x, y, w, h);
        lv_obj_t* t = makeLabel(*card, title, COL_DIM, &lv_font_montserrat_12);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 4, 2);

        *valLbl = makeLabel(*card, "--", COL_TEXT, valFont);
        lv_obj_center(*valLbl);

        lv_obj_t* u = makeLabel(*card, unitsLabel().c_str(),
                                COL_DIM, &lv_font_montserrat_12);
        lv_obj_align(u, LV_ALIGN_BOTTOM_MID, 0, -1);
    };

    buildCard(&_cardPrev, &_lblPrev, prevX, rowY, sideW, rowH,
              T(S_PREV), &lv_font_montserrat_24);
    buildCard(&_cardCur,  &_lblCur,  curX,  rowY, curW,  rowH,
              T(S_CURRENT), &lv_font_montserrat_40);
    buildCard(&_cardNext, &_lblNext, nextX, rowY, sideW, rowH,
              T(S_NEXT), &lv_font_montserrat_24);

    // ---- Chart ----
    int chY = 108, chH = 100;
    lv_obj_t* chartCard = makeCard(_scrMain, 4, chY, DISP_W - 8, chH);

    _chart = lv_chart_create(chartCard);
    lv_obj_set_size(_chart, DISP_W - 16, chH - 8);
    lv_obj_align(_chart, LV_ALIGN_CENTER, 0, 0);
    lv_chart_set_type(_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(_chart, 48);
    lv_chart_set_div_line_count(_chart, 3, 0);
    lv_obj_set_style_bg_opa(_chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_chart, 0, 0);
    lv_obj_set_style_pad_all(_chart, 0, 0);
    lv_obj_set_style_pad_column(_chart, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(_chart, 0, LV_PART_ITEMS);
    _chartSeries = lv_chart_add_series(_chart, COL_GREEN, LV_CHART_AXIS_PRIMARY_Y);

    _chartUnits = makeLabel(chartCard, unitsLabel().c_str(),
                            COL_DIM, &lv_font_montserrat_12);
    lv_obj_align(_chartUnits, LV_ALIGN_TOP_LEFT, 2, 0);

    // ---- Footer stats (4 mini cards) ----
    int ftY = 212, ftH = 56;
    int fW = (DISP_W - 5 * padX) / 4;

    auto buildStat = [&](lv_obj_t** lbl, int idx, const char* title) {
        int x = padX + idx * (fW + padX);
        lv_obj_t* c = makeCard(_scrMain, x, ftY, fW, ftH);
        lv_obj_t* t = makeLabel(c, title, COL_DIM, &lv_font_montserrat_12);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 2, 1);
        *lbl = makeLabel(c, "--", COL_TEXT, &lv_font_montserrat_20);
        lv_obj_align(*lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
    };
    buildStat(&_statAvg, 0, T(S_PRICES_TODAY));
    buildStat(&_statMin, 1, T(S_MIN_PRICE));
    buildStat(&_statMax, 2, T(S_MAX_PRICE));
    buildStat(&_statUpd, 3, T(S_LAST_UPDATE));
}

// ---------- RELAY GRID SCREEN ----------------------------------------

void UI::buildRelays() {
    _scrRelays = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrRelays, COL_BG, 0);
    lv_obj_set_style_bg_opa(_scrRelays, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_scrRelays, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_scrRelays, 0, 0);

    // Header strip
    lv_obj_t* title = makeLabel(_scrRelays, T(S_RELAYS),
                                COL_TEXT, &lv_font_montserrat_16);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 6);

    lv_obj_t* back = makeCard(_scrRelays, DISP_W - 80, 4, 72, 24);
    lv_obj_set_style_radius(back, 12, 0);
    lv_obj_t* bl = makeLabel(back, T(S_BACK), COL_TEXT, &lv_font_montserrat_12);
    lv_obj_center(bl);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, onBackToMainTouched, LV_EVENT_CLICKED, this);

    // 4×2 grid for 8 relays. With DISP_W=480, DISP_H=272 minus 36px header,
    // each tile is ~115×115 — finger-friendly.
    int top = 36;
    int gridH = DISP_H - top - 4;
    int cols = 4, rows = 2;
    int padX = 6, padY = 6;
    int tileW = (DISP_W - padX * (cols + 1)) / cols;
    int tileH = (gridH - padY * (rows + 1)) / rows;

    for (int i = 0; i < NUM_RELAYS; ++i) {
        int r = i / cols, c = i % cols;
        int tx = padX + c * (tileW + padX);
        int ty = top + r * (tileH + padY);

        RelayTile& tt = _tiles[i];
        tt.card = makeCard(_scrRelays, tx, ty, tileW, tileH);
        lv_obj_add_flag(tt.card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(tt.card, (void*)(intptr_t)i);
        lv_obj_add_event_cb(tt.card, onRelayTileTouched, LV_EVENT_CLICKED, this);

        tt.name = makeLabel(tt.card, "Relay", COL_TEXT, &lv_font_montserrat_14);
        lv_obj_align(tt.name, LV_ALIGN_TOP_LEFT, 2, 2);

        tt.state = makeLabel(tt.card, "OFF", COL_DIM, &lv_font_montserrat_12);
        lv_obj_align(tt.state, LV_ALIGN_TOP_RIGHT, -2, 2);

        tt.mode = makeLabel(tt.card, "Always OFF", COL_DIM, &lv_font_montserrat_12);
        lv_obj_align(tt.mode, LV_ALIGN_TOP_LEFT, 2, 26);

        tt.threshold = makeLabel(tt.card, "", COL_TEXT, &lv_font_montserrat_16);
        lv_obj_align(tt.threshold, LV_ALIGN_BOTTOM_LEFT, 2, -4);
    }
}

// ---------- EDIT SCREEN ----------------------------------------------

void UI::buildEdit() {
    _scrEdit = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrEdit, COL_BG, 0);
    lv_obj_set_style_bg_opa(_scrEdit, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_scrEdit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_scrEdit, 0, 0);

    _editTitle = makeLabel(_scrEdit, "Relay 1", COL_TEXT, &lv_font_montserrat_16);
    lv_obj_align(_editTitle, LV_ALIGN_TOP_LEFT, 8, 6);

    lv_obj_t* back = makeCard(_scrEdit, DISP_W - 80, 4, 72, 24);
    lv_obj_set_style_radius(back, 12, 0);
    lv_obj_t* bl = makeLabel(back, T(S_BACK), COL_TEXT, &lv_font_montserrat_12);
    lv_obj_center(bl);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, onBackToRelaysTouched, LV_EVENT_CLICKED, this);

    // 4 mode buttons across the top
    const char* labels[4] = { T(S_ALWAYS_OFF), T(S_ALWAYS_ON),
                              T(S_TURN_ON_BELOW), T(S_TURN_OFF_ABOVE) };
    int btnY = 36, btnH = 56;
    int gap = 6;
    int btnW = (DISP_W - 5 * gap) / 4;
    for (int m = 0; m < 4; ++m) {
        int bx = gap + m * (btnW + gap);
        lv_obj_t* b = makeCard(_scrEdit, bx, btnY, btnW, btnH);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(b, (void*)(intptr_t)m);
        lv_obj_add_event_cb(b, onModeBtnTouched, LV_EVENT_CLICKED, this);
        lv_obj_t* l = makeLabel(b, labels[m], COL_TEXT, &lv_font_montserrat_12);
        lv_obj_center(l);
        _modeBtns[m] = b;
    }

    // Threshold spinner row at the bottom: [-]  value  [+]
    int valY = 100, valH = 80;
    int spinW = 70;

    lv_obj_t* minus = makeCard(_scrEdit, 6, valY, spinW, valH);
    lv_obj_add_flag(minus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(minus, onMinusTouched, LV_EVENT_CLICKED, this);
    lv_obj_t* mlbl = makeLabel(minus, "-", COL_TEXT, &lv_font_montserrat_48);
    lv_obj_center(mlbl);

    lv_obj_t* plus = makeCard(_scrEdit, DISP_W - 6 - spinW, valY, spinW, valH);
    lv_obj_add_flag(plus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(plus, onPlusTouched, LV_EVENT_CLICKED, this);
    lv_obj_t* plbl = makeLabel(plus, "+", COL_TEXT, &lv_font_montserrat_48);
    lv_obj_center(plbl);

    lv_obj_t* valCard = makeCard(_scrEdit, 6 + spinW + 6, valY,
                                  DISP_W - 2 * (6 + spinW + 6), valH);
    _editValueLabel = makeLabel(valCard, "--", COL_TEXT, &lv_font_montserrat_28);
    lv_obj_center(_editValueLabel);

    lv_obj_t* tlbl = makeLabel(_scrEdit, T(S_THRESHOLD),
                                COL_DIM, &lv_font_montserrat_12);
    lv_obj_align(tlbl, LV_ALIGN_BOTTOM_LEFT, 8, -8);
}

// ---- Refresh (called periodically) ----------------------------------

void UI::refresh() {
    refreshHeader();

    if (lv_scr_act() == _scrMain) {
        refreshPriceCards();
        refreshChart();
        refreshStats();
    } else if (lv_scr_act() == _scrRelays) {
        refreshRelayTiles();
    } else if (lv_scr_act() == _scrEdit) {
        refreshEdit();
    }
}

void UI::refreshHeader() {
    // Status dot color & IP/clock text
    lv_color_t dotCol = _configMode ? COL_ORANGE
                                    : (_wifi ? COL_GREEN : COL_RED);
    lv_obj_set_style_bg_color(_hdrStatus, dotCol, 0);

    String ipStr = _configMode ? String(T(S_CONFIG_MODE)) + " " + _ip
                               : (_wifi ? _ip : String(T(S_OFFLINE)));
    lv_label_set_text(_hdrIp, ipStr.c_str());

    String clk = currentTimeString();
    if (clk != _lastClock) {
        lv_label_set_text(_hdrClock, clk.c_str());
        _lastClock = clk;
    }
    lv_label_set_text(_hdrDate, dateString().c_str());
}

void UI::refreshPriceCards() {
    time_t now = time(nullptr);
    const PriceEntry* prv = _prices.getPrevious(now);
    const PriceEntry* cur = _prices.getCurrent(now);
    const PriceEntry* nxt = _prices.getNext(now);

    float dMin = 0, dMax = 0, dAvg = 0;
    bool hasDay = _prices.statsForDay(now, dMin, dMax, dAvg);

    auto setVal = [&](lv_obj_t* lbl, const PriceEntry* e) {
        if (!e) { lv_label_set_text(lbl, "--");
                  lv_obj_set_style_text_color(lbl, COL_TEXT, 0); return; }
        lv_label_set_text(lbl, formatPrice(e->price_raw).c_str());
        lv_color_t c = hasDay ? colorForPrice(e->price_raw, dMin, dMax) : COL_TEXT;
        lv_obj_set_style_text_color(lbl, c, 0);
    };
    setVal(_lblPrev, prv);
    setVal(_lblCur,  cur);
    setVal(_lblNext, nxt);
}

void UI::refreshChart() {
    int n = _prices.count();
    if (n == 0) return;

    // Build a 48-entry window starting at start-of-today
    time_t now = time(nullptr);
    struct tm tnow; localtime_r(&now, &tnow);
    tnow.tm_hour = 0; tnow.tm_min = 0; tnow.tm_sec = 0;
    time_t todayStart = mktime(&tnow);

    int first = -1, last = -1;
    for (int i = 0; i < n; ++i) {
        time_t ts = _prices.at(i).ts_start;
        if (ts >= todayStart && ts < todayStart + 48 * 3600) {
            if (first < 0) first = i;
            last = i;
        }
    }
    if (first < 0) return;

    float mn = _prices.at(first).price_raw, mx = mn;
    for (int i = first; i <= last; ++i) {
        float p = _prices.at(i).price_raw;
        if (p < mn) mn = p;
        if (p > mx) mx = p;
    }
    float lo = mn < 0 ? mn : 0;
    float hi = mx * 1.05f;
    if (hi - lo < 0.01f) hi = lo + 0.01f;

    // LVGL's chart uses integer values; scale to cents/kWh (×100) for resolution.
    int yMin = (int)(lo * 100.0f * 10.0f);   // 1 unit = 0.001 c/kWh
    int yMax = (int)(hi * 100.0f * 10.0f);
    lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y, yMin, yMax);
    lv_chart_set_point_count(_chart, last - first + 1);

    int nowIdx = -1;
    for (int i = first; i <= last; ++i) {
        if (now >= _prices.at(i).ts_start &&
            now <  _prices.at(i).ts_start + 3600) { nowIdx = i; break; }
    }

    for (int i = first; i <= last; ++i) {
        float p = _prices.at(i).price_raw;
        int v = (int)(p * 1000.0f);
        lv_chart_set_next_value(_chart, _chartSeries, v);
    }
    // Color of the series stays single-tone for now; per-bar coloring isn't
    // exposed by LVGL's chart widget without a custom draw callback.
    lv_chart_set_series_color(_chart, _chartSeries, COL_NOWBAR);
    _lastNowIdx = nowIdx;

    lv_label_set_text(_chartUnits, unitsLabel().c_str());
}

void UI::refreshStats() {
    time_t now = time(nullptr);
    float mn, mx, av;
    bool hasDay = _prices.statsForDay(now, mn, mx, av);

    if (hasDay) {
        lv_label_set_text(_statAvg, formatPrice(av).c_str());
        lv_label_set_text(_statMin, formatPrice(mn).c_str());
        lv_label_set_text(_statMax, formatPrice(mx).c_str());
        lv_obj_set_style_text_color(_statMin, COL_GREEN, 0);
        lv_obj_set_style_text_color(_statMax, COL_ORANGE, 0);
    } else {
        lv_label_set_text(_statAvg, "--");
        lv_label_set_text(_statMin, "--");
        lv_label_set_text(_statMax, "--");
    }

    time_t lu = _prices.lastUpdate();
    if (lu > 100000) {
        struct tm t; localtime_r(&lu, &t);
        char b[8]; snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
        lv_label_set_text(_statUpd, b);
    } else {
        lv_label_set_text(_statUpd, "--");
    }
}

void UI::refreshRelayTiles() {
    for (int i = 0; i < NUM_RELAYS; ++i) {
        const RelayRule& rr = _relays.rule(i);
        RelayTile& tt = _tiles[i];
        lv_label_set_text(tt.name, rr.name);
        lv_label_set_text(tt.state, rr.state ? T(S_ON) : T(S_OFF));
        lv_obj_set_style_text_color(tt.state,
            rr.state ? COL_GREEN : COL_DIM, 0);

        const char* m = "";
        switch (rr.mode) {
            case RMODE_ALWAYS_OFF: m = T(S_ALWAYS_OFF); break;
            case RMODE_ALWAYS_ON:  m = T(S_ALWAYS_ON);  break;
            case RMODE_ON_BELOW:   m = T(S_TURN_ON_BELOW); break;
            case RMODE_OFF_ABOVE:  m = T(S_TURN_OFF_ABOVE); break;
        }
        lv_label_set_text(tt.mode, m);

        if (rr.mode == RMODE_ON_BELOW || rr.mode == RMODE_OFF_ABOVE) {
            char b[16]; snprintf(b, sizeof(b), "%.1f c", rr.threshold * 100.0f);
            lv_label_set_text(tt.threshold, b);
        } else {
            lv_label_set_text(tt.threshold, "");
        }

        // Highlight border when ON
        lv_obj_set_style_border_color(tt.card,
            rr.state ? COL_GREEN : COL_CARD, 0);
        lv_obj_set_style_border_width(tt.card, rr.state ? 2 : 0, 0);
    }
}

void UI::refreshEdit() {
    if (_editIdx < 0 || _editIdx >= NUM_RELAYS) return;
    const RelayRule& rr = _relays.rule(_editIdx);

    char title[40];
    snprintf(title, sizeof(title), "%s %d  %s", T(S_RELAY), _editIdx + 1, rr.name);
    lv_label_set_text(_editTitle, title);

    for (int m = 0; m < 4; ++m) {
        bool active = (int)rr.mode == m;
        lv_obj_set_style_bg_color(_modeBtns[m],
            active ? COL_NOWBAR : COL_CARD, 0);
    }

    char vb[32];
    snprintf(vb, sizeof(vb), "%.1f c/kWh", rr.threshold * 100.0f);
    lv_label_set_text(_editValueLabel, vb);
}

// ---- Public state setters ------------------------------------------

void UI::setWifiConnected(bool c)    { _wifi = c; }
void UI::setIpString(const String& ip) { _ip = ip; }
void UI::setConfigMode(bool c)       { _configMode = c; }
void UI::setShowVat(bool v)          { _showVat = v; }
void UI::setVatPercent(float v)      { _vat = v; }

// ---- Navigation -----------------------------------------------------

void UI::openMain()             { lv_scr_load(_scrMain); }
void UI::openRelays()           { lv_scr_load(_scrRelays); }
void UI::openEdit(int idx)      { _editIdx = idx; lv_scr_load(_scrEdit); }

// ---- Event thunks ---------------------------------------------------
// LVGL passes a context pointer through user_data; we cast it to UI* and
// call the right method. This keeps event handling cohesive without
// forcing us to make every method public.

void UI::onMainTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    self->openRelays();
}

void UI::onRelayTileTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    lv_obj_t* tile = lv_event_get_current_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(tile);
    self->openEdit(idx);
}

void UI::onBackToMainTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    self->openMain();
}

void UI::onBackToRelaysTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    self->openRelays();
}

void UI::onModeBtnTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_current_target(e);
    int m = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (self->_editIdx >= 0 && self->_editIdx < NUM_RELAYS) {
        self->_relays.rule(self->_editIdx).mode = (RelayMode)m;
        self->_relays.save();
        self->refreshEdit();
    }
}

void UI::onMinusTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    if (self->_editIdx < 0 || self->_editIdx >= NUM_RELAYS) return;
    RelayRule& rr = self->_relays.rule(self->_editIdx);
    rr.threshold -= 0.005f;
    if (rr.threshold < 0) rr.threshold = 0;
    self->_relays.save();
    self->refreshEdit();
}

void UI::onPlusTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    if (self->_editIdx < 0 || self->_editIdx >= NUM_RELAYS) return;
    RelayRule& rr = self->_relays.rule(self->_editIdx);
    rr.threshold += 0.005f;
    if (rr.threshold > 2.0f) rr.threshold = 2.0f;
    self->_relays.save();
    self->refreshEdit();
}
